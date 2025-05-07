// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

const { app, BrowserWindow, ipcMain, session } = require("electron");
const path = require("node:path");
const fs = require('fs');
const https = require("node:https");
const { randomUUID } = require("node:crypto");

const JsonRpcClient = require("./json_rpc_client").JsonRpcClient;
const Engine = require("./engine.js").Engine;
const Integration = require("./integration.js").Integration;
const constants = require("./constants.js");
const { readFileSync } = require("node:fs");
const axios = require('axios');

const axiosInstance = axios.create({
  httpsAgent: new https.Agent({
    rejectUnauthorized: false
  })
})

class IntegrationContext {
    integration = null;
    engineContextById = {};
    deviceAgentPushDataContext = null;

    constructor(manifests, mouseMovableObjectMetadata, rpcClient) {
        this.integration = new Integration(manifests, mouseMovableObjectMetadata, rpcClient);
        this.integrationUserName = null;
        this.integrationPassword = null;
        this.integrationUserId = null;
    }
};

class EngineContext {
    engine = null;
    deviceAgentsByDeviceId = {};

    constructor(engineId, mouseMovableObjectMetadata, appContext, manifests) {
        this.engine = new Engine(engineId, mouseMovableObjectMetadata, appContext, manifests);
    }
}

class AppContext  {
    manifests = {};
    config = null;
    integrationContext = null;
    rpcClient = null;
    window = null;
    mouseMovableObjectMetadata = {
        typeId: "analytics.api.stub.object.type",
        trackId: randomUUID(),
        boundingBox: {
            x: 0,
            y: 0,
            width: 0,
            height: 0
        },
        attributes: [
            {
                name: "attribute 1",
                value: "attribute value 1",
            },
            {
                name: "attribute 2",
                value: "attribute value 2",
            },
            {
                name: "attribute 3",
                value: "attribute value 3",
            }
        ]
    };

    viewModel = {
        connectionState: "disconnected"
    };

    constructor(config, manifests, rpcClient) {
        console.log("Initializing AppContext.");

        this.manifests = manifests;
        this.config = config;
        this.integrationContext = new IntegrationContext(
            manifests,
            this.mouseMovableObjectMetadata,
            rpcClient);
        this.rpcClient = rpcClient;
        this.adminSessionToken = null;
        this.integrationUserSessionToken = null;
    }
};

let appContext = null;

app.on('ready', () => {
    // Ignore SSL certificate errors
    app.on('certificate-error', (event, webContents, url, error, certificate, callback) => {
        event.preventDefault();
        callback(true); // Bypass the error
    });
});

const initializeWebRtc = (appContext, deviceId) => {
    if (appContext.adminSessionToken === null)
    {
        console.log("initializeWebRtc: Admin session token is null");
        return;
    }

    const config = JSON.parse(JSON.stringify(appContext.config));
    config.adminSessionToken = appContext.adminSessionToken;
    config.deviceId = deviceId;

    appContext.window.webContents.send("initialize-webrtc", config);
}

const createWindow = () => {
    const window = new BrowserWindow({
        width: 800,
        height: 600,
        webPreferences: {
            preload: path.resolve(__dirname, "preload.js"),
            nodeIntegration: true,
            contextIsolation: true,
            enableRemoteModule: true,
        }
    });

    window.once('ready-to-show', () => {
    });

    window.loadFile("index.html");
    window.webContents.openDevTools();
    return window;
}

function findOrCreateEngineContextById(engineId)
{
    const engineContexts = appContext.integrationContext.engineContextById;

    if (!engineContexts.hasOwnProperty(engineId))
    {
        engineContexts[engineId] =
            new EngineContext(engineId, appContext.mouseMovableObjectMetadata, appContext,
                appContext.manifests);
    }

    return engineContexts[engineId];
}

function findOrCreateEngineById(engineId)
{
    const engineContext = findOrCreateEngineContextById(engineId);

    return engineContext.engine;
}

function findDeviceAgentByIdOrThrow(engineId, deviceId)
{
    const engineContext = findOrCreateEngineContextById(engineId);

    const deviceAgents = engineContext.deviceAgentsByDeviceId;

    if (!deviceAgents.hasOwnProperty(deviceId))
    {
        throw new Error('Device Agent not found');
    }

    return deviceAgents[deviceId];
}

const initialize = async () => {
    console.log("Initializing application.");

    let config = JSON.parse(readFileSync("config.json"));

    let manifests = {
        integrationManifest: JSON.parse(readFileSync("integration_manifest.json", "utf8")),
        engineManifest: JSON.parse(readFileSync("engine_manifest.json", "utf8")),
        deviceAgentManifest: JSON.parse(readFileSync("device_agent_manifest.json", "utf8"))
    };

    console.log("Read config:", config);
    console.log("Read manifests:", manifests);

    appContext = new AppContext(
        config,
        manifests,
        await initializeRpcClient()
        );

    if (fs.existsSync("integration_credentials.json"))
    {
        console.log('integration_credentials.json exists, reading data from it');

        let data = JSON.parse(readFileSync("integration_credentials.json"));

        console.log("Read data:", data);

        appContext.integrationContext.integrationUserName = data.integrationUserName;
        appContext.integrationContext.integrationPassword = data.integrationPassword;
        appContext.integrationContext.integrationUserId = data.integrationUserId;
    }
    else
    {
        console.log("integration_credentials.json doesn't exist, make integration request and approve it first");
    }

    if (fs.existsSync("integration_state.json"))
    {
        console.log('integration_state.json exists, reading data from it');

        let integration_state = JSON.parse(readFileSync("integration_state.json"));

        console.log("Read data:", integration_state);

        for (const [deviceId, data] of Object.entries(integration_state))
        {
            let engineId = data.engineId;
            let deviceInfo = data.deviceInfo;

            const engineContext = findOrCreateEngineContextById(engineId);
            const deviceAgents = engineContext.deviceAgentsByDeviceId;

            if (!deviceAgents.hasOwnProperty(deviceId))
            {
                const engine = engineContext.engine;
                const deviceAgent = engine.obtainDeviceAgent(deviceInfo);
                deviceAgents[deviceId] = deviceAgent;
            }
        }
    }

    console.log("Logging in as Admin and trying to get admin token.");

    appContext.adminSessionToken = await getSessionToken(
        appContext.config.vmsUser,
        appContext.config.vmsAdminPassword);
}

const getSessionToken = async (user, password) => {
    console.log("Getting session token.");

    const path = "rest/v1/login/sessions";
    const route = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${path}`;

    const data = {
        username: user,
        password: password
    }

    const config = {
        headers: {
            'Content-Type': 'application/json'
        },
        httpsAgent: new https.Agent({
            rejectUnauthorized: false, //< Disables certificate validation.
        })
    };

    console.log("Request route: ", route);
    console.log("Config: ", config);
    console.log("Data: ", data);

    try
    {
        const response = await axiosInstance.post(route, data, config);
        console.log('Response:', response.data);
        return response.data.token;
    }
    catch (error)
    {
        console.error('Error:', error);
        return null;
    }
}

const authorizeIntegrationAndConnect = async () => {
    console.log("Authorizing integration and connecting.");
    const url =
        "wss://"
        + appContext.config.vmsHost
        + ":"
        + appContext.config.vmsPort
        + "/jsonrpc";

    appContext.integrationUserSessionToken = await getSessionToken(
        appContext.integrationContext.integrationUserName,
        appContext.integrationContext.integrationPassword);

    if (!appContext.integrationUserSessionToken)
    {
        console.log("Failed to get integration user session token.");
        return false;
    }

    appContext.rpcClient.connect(url.toString(), {
        rejectUnauthorized: false,
        headers: {
            "Authorization": "Bearer " + appContext.integrationUserSessionToken
        }
    });

    return true;
}

const initializeRpcClient = async () => {
    console.log("Initializing RPC client.");
    let rpcClient = new JsonRpcClient();

    rpcClient.on(constants.CREATE_DEVICE_AGENT_METHOD, async (data) => {
        console.log("Device Agent requested, Device Agent info:", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;
        const deviceInfo = data.params.parameters;
        const deviceId = deviceInfo.id;

        const engineContext = findOrCreateEngineContextById(engineId);
        const deviceAgents = engineContext.deviceAgentsByDeviceId;

        function initializePushData(engine, deviceAgent)
        {
            if (!appContext.integrationContext.deviceAgentPushDataContext)
            {
                appContext.integrationContext.deviceAgentPushDataContext =
                {
                    deviceAgent: deviceAgent,
                    engine: engine,
                    frameCount: 0,
                    pdeFromEngine: true, //< True - from Engine, false - from DeviceAgent.
                    pushDataInfo: { timestampMs: 1 },
                    connected: true
                };
            }
        }

        if (!deviceAgents.hasOwnProperty(deviceId))
        {
            const engine = engineContext.engine;
            const deviceAgent = engine.obtainDeviceAgent(deviceInfo);
            deviceAgents[deviceId] = deviceAgent;
        }

        initializePushData(engineContext.engine, deviceAgents[deviceId]);
        initializeWebRtc(appContext, deviceId);

        if (!fs.existsSync("integration_state.json"))
            fs.writeFileSync("integration_state.json", JSON.stringify({}));

        let integration_state = JSON.parse(readFileSync("integration_state.json"));

        integration_state[deviceId] = {
            engineId: engineId,
            deviceInfo: deviceInfo
        };

        fs.writeFileSync("integration_state.json", JSON.stringify(integration_state));

        return {
            type: "ok",
            data: {
                deviceAgentManifest: deviceAgents[deviceId].manifest()
            }
        };
    });

    rpcClient.on(constants.DELETE_DEVICE_AGENT_METHOD, async (data) => {
        console.log("Delete Device Agent", data);

        const engineId = data.params.target.engineId;
        const deviceId = data.params.target.deviceId;

        let engineContext = findOrCreateEngineContextById(engineId);
        let deviceAgents = engineContext.deviceAgentsByDeviceId;

        if (deviceAgents.hasOwnProperty(deviceId))
        {
            if (fs.existsSync("integration_state.json"))
            {
                let integration_state = JSON.parse(readFileSync("integration_state.json"));
                delete integration_state[deviceId];
                fs.writeFileSync("integration_state.json", JSON.stringify(integration_state));
            }

            if (appContext.integrationContext.deviceAgentPushDataContext)
            {
                console.log("Stopping push data loop.");
                appContext.integrationContext.deviceAgentPushDataContext = null;
            }
            else
            {
                console.log("Push data loop is already stopped.");
            }
        }
    });

    rpcClient.on(constants.SET_ENGINE_SETTINGS_METHOD, async (data) => {
        console.log("Engine setSettings: ", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;
        const settingsRequest = data.params.parameters;
        const settingsValues = settingsRequest.settingsValues;

        const engine = findOrCreateEngineById(engineId);

        const settingsResponse = engine.setSettings(settingsValues);

        console.log("Response: ", JSON.stringify(settingsResponse, null, 4));

        return settingsResponse;
    });

    rpcClient.on(constants.SET_DEVICE_AGENT_SETTINGS_METHOD, async (data) => {
        console.log("Device Agent setSettings: ", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;
        const deviceId = data.params.target.deviceId;
        const settingsRequest = data.params.parameters;
        const settingsValues = settingsRequest.settingsValues;

        const deviceAgent = findDeviceAgentByIdOrThrow(engineId, deviceId);

        let settingsResponse = deviceAgent.setSettings(settingsValues);

        console.log("Response: ", JSON.stringify(settingsResponse, null, 4));

        return settingsResponse;
    });

    rpcClient.on(constants.GET_ENGINE_SETTINGS_ON_ACTIVE_SETTING_CHANGE, async (data) => {
        console.log("Engine getSettingsOnActiveSettingChange: ", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;
        const activeSettingChangedAction = data.params.parameters;

        const engine = findOrCreateEngineById(engineId);

        const activeSettingChangedResponse =
            engine.getSettingsOnActiveSettingChange(activeSettingChangedAction);

        console.log("Response: ", JSON.stringify(activeSettingChangedResponse, null, 4));

        return activeSettingChangedResponse;
    });

    rpcClient.on(constants.GET_DEVICE_AGENT_SETTINGS_ON_ACTIVE_SETTING_CHANGE, async (data) => {
        console.log("Device Agent getSettingsOnActiveSettingChange: ", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;
        const deviceId = data.params.target.deviceId;
        const activeSettingChangedAction = data.params.parameters;

        const deviceAgent = findDeviceAgentByIdOrThrow(engineId, deviceId);

        const activeSettingChangedResponse =
            deviceAgent.getSettingsOnActiveSettingChange(activeSettingChangedAction);

        console.log("Response: ", JSON.stringify(activeSettingChangedResponse, null, 4));

        return activeSettingChangedResponse;
    });

    rpcClient.on(constants.EXECUTE_ENGINE_ACTION, async (data) => {
        console.log("Engine executeEngineAction: ", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;
        const action = data.params.parameters;

        const engine = findOrCreateEngineById(engineId);

        const actionResult = engine.executeAction(action);

        console.log("Result: ", JSON.stringify(actionResult, null, 4));

        return actionResult;
    });

    rpcClient.on(constants.GET_ENGINE_COMPATIBILITY_INFO_METHOD, async (data) => {
        console.log("Engine compatibility info: ", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;
        const deviceInfo = data.params.parameters;

        const engine = findOrCreateEngineById(engineId);

        const result = engine.compatibilityInfo(deviceInfo);

        console.log("Result: ", JSON.stringify(result, null, 4));

        return result;
    });

    rpcClient.on(constants.GET_ENGINE_SIDE_SETTINGS_METHOD, async (data) => {
        console.log("Engine side settings: ", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;

        const engine = findOrCreateEngineById(engineId);

        const result = engine.getIntegrationSideSettings();

        console.log("Result: ", JSON.stringify(result, null, 4));

        return result;
    });

    rpcClient.on(constants.GET_DEVICE_AGENT_SIDE_SETTINGS_METHOD, async (data) => {
        console.log("Device Agent side settings: ", JSON.stringify(data, null, 4));

        const engineId = data.params.target.engineId;
        const deviceId = data.params.target.deviceId;

        const deviceAgent = findDeviceAgentByIdOrThrow(engineId, deviceId);

        const result = deviceAgent.getIntegrationSideSettings();

        console.log("Result: ", JSON.stringify(result, null, 4));

        return result;
    });

    rpcClient.onClose(async () => {
        // This handler will be called only when the server closes the websocket connection.
        console.log("Executing onClose handler.");

        // In case the session is expired, try to refresh the session and reconnect.
        if (await authorizeIntegrationAndConnect())
            return;

        appContext.viewModel.connectionState = "disconnected";
        appContext.window.webContents.send("state-changed", appContext.viewModel);
    });

    rpcClient.onOpen(() => {
        console.log("Executing onOpen handler.");

        if (!appContext.integrationContext.integrationUserId)
        {
            console.log("Make integration request and approve it first.");
            return;
        }

        let userId =  {
            id: appContext.integrationContext.integrationUserId
        };

        rpcClient.call(constants.GET_USER_METHOD, userId)
        .then((result) => {
            console.log("Getting user info: ", JSON.stringify(result, null, 4));

            if (result.result.parameters.integrationRequestData.isApproved)
            {
                console.log("Integration request is approved, subscribing.");

                rpcClient.call(constants.SUBSCRIBE)
                .then((result) => {
                    console.log("Result:", result);
                    appContext.viewModel.connectionState = "connected";
                    console.log("Sending 'state-changed' event");
                    appContext.window.webContents.send("state-changed", appContext.viewModel);
                }).catch((error) => {
                    console.log(error);
                });
            }
            else
            {
                console.log("Integration request is not approved. Approve it first.");
            }
        }).catch((error) => {
            console.log(error);
        });

    });

    return rpcClient;
}

const initializeNotifications = () => {
    ipcMain.on("object-move", (event, object) => {
        console.log(object);
        const boundingBox = appContext.mouseMovableObjectMetadata.boundingBox;

        boundingBox.x = object.x;
        boundingBox.y = object.y;
        boundingBox.width = object.width;
        boundingBox.height = object.height;
    });

    ipcMain.on("connect-disconnect", async () => {
        if (appContext.viewModel.connectionState == "connected")
        {
            console.log("Disconnecting.");
            if (appContext.integrationContext.deviceAgentPushDataContext)
            {
                appContext.integrationContext.deviceAgentPushDataContext.connected = false;
            }
            // Disconnect without executing the onClose handler.
            // When the onClose handler is executed, it will try to reconnect, but we don't need
            // to reconnect when the user explicitly disconnects by pressing a button, we only
            // need to reconnect when the websocket connection is closed by the server.
            appContext.rpcClient.disconnect();
            appContext.viewModel.connectionState = "disconnected";
            appContext.window.webContents.send("state-changed", appContext.viewModel);
        }
        else
        {
            console.log("Connecting.");

            if (!appContext.adminSessionToken)
            {
                console.log("Not logged in as admin");
                return;
            }

            if (!appContext.integrationContext.integrationUserName)
            {
                console.log("Make integration request first.");
                return;
            }

            appContext.rpcClient = await initializeRpcClient();

            await authorizeIntegrationAndConnect();

            if (appContext.integrationContext.deviceAgentPushDataContext)
            {
                appContext.integrationContext.deviceAgentPushDataContext.connected = true;
            }
        }
    });

    ipcMain.on("make-integration-request", async () => {
        console.log("Making integration request.");

        if (!appContext.adminSessionToken)
        {
            console.log("Not logged in as admin");
            return;
        }

        const path = "rest/v4/analytics/integrations/*/requests";
        const route = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${path}`;

        const data = {
            integrationManifest: appContext.integrationContext.integration.manifest(),
            engineManifest: appContext.manifests.engineManifest,
            pinCode: "1234"
        };

        const config = {
            headers: {
                'Content-Type': 'application/json'
            },
            httpsAgent: new https.Agent({
                rejectUnauthorized: false, //< Disables certificate validation.
            })
        };

        console.log("Request route: ", route);
        console.log("Request data: ", data);
        console.log("Config: ", config);

        try
        {
            const response = await axiosInstance.post(route, data, config);
            console.log('Response:', response.data);
            appContext.integrationContext.integrationUserName = response.data.username;
            appContext.integrationContext.integrationPassword = response.data.password;
            appContext.integrationContext.integrationUserId = response.data.requestId;

            const integration_credentials = {
                integrationUserName: appContext.integrationContext.integrationUserName,
                integrationPassword: appContext.integrationContext.integrationPassword,
                integrationUserId: appContext.integrationContext.integrationUserId
            }

            console.log("Writing integration_credentials.json, data:", integration_credentials);
            fs.writeFileSync("integration_credentials.json", JSON.stringify(integration_credentials));
            fs.writeFileSync("integration_state.json", JSON.stringify({}));
        }
        catch (error)
        {
            console.error('Error:', error);
        }
    });

    ipcMain.on("approve-integration-request", async () => {
        console.log("Approving integration request.");

        if (!appContext.adminSessionToken)
        {
            console.log("Not logged in as admin");
            return;
        }

        if (!appContext.integrationContext.integrationUserId)
        {
            console.log("Make integration request first.");
            return;
        }

        const integrationUserId = appContext.integrationContext.integrationUserId;
        const path = `rest/v4/analytics/integrations/*/requests/${integrationUserId}/approve`;
        const route = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${path}`;

        const data = {
            requestId: integrationUserId
        }

        const config = {
            headers: {
                'Content-Type': 'application/json',
                "Authorization": "Bearer " + appContext.adminSessionToken
            },
            httpsAgent: new https.Agent({
                rejectUnauthorized: false, //< Disables certificate validation.
            })
        };

        console.log("Request route: ", route);
        console.log("Config: ", config);
        console.log("Data: ", data);

        try
        {
            const response = await axiosInstance.post(route, data, config);
            console.log('Response:', response.data);
        }
        catch (error)
        {
            console.error('Error:', error);
        }
    });

    ipcMain.on("remove-integration", async () => {
        console.log("Removing integration.");

        if (!appContext.adminSessionToken)
        {
            console.log("Not logged in as admin");
            return;
        }

        if (!appContext.integrationContext.integrationUserId)
        {
            console.log("Make integration request and approve it first.");
            return;
        }

        const integrationUserId = appContext.integrationContext.integrationUserId;
        const usersPath = `rest/v4/users/${integrationUserId}`;
        const usersRoute = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${usersPath}`;

        const usersData = {
            id: integrationUserId
        };

        const usersConfig = {
            headers: {
                'Content-Type': 'application/json',
                "Authorization": "Bearer " + appContext.adminSessionToken
            },
            httpsAgent: new https.Agent({
                rejectUnauthorized: false, //< Disables certificate validation.
            }),
            data: usersData
        };

        let integrationId = null;

        try
        {
            console.log("Request route: ", usersRoute);
            console.log("Config: ", usersConfig);

            const response = await axiosInstance.get(usersRoute, usersConfig);
            console.log('Response:', response.data);

            integrationId = response.data.parameters.integrationRequestData.integrationId;
        }
        catch (error)
        {
            console.error('Error:', error);
            return;
        }

        const path = `rest/v4/analytics/integrations/${integrationId}`;
        const route = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${path}`;

        const data = {
            id: integrationId
        };

        const config = {
            headers: {
                'Content-Type': 'application/json',
                "Authorization": "Bearer " + appContext.adminSessionToken
            },
            httpsAgent: new https.Agent({
                rejectUnauthorized: false, //< Disables certificate validation.
            }),
            data: data
        };

        try
        {
            console.log("Request route: ", route);
            console.log("Request data: ", data);
            console.log("Config: ", config);

            const response = await axiosInstance.delete(route, config);
            console.log('Response:', response.data);

            appContext.integrationContext.integrationUserName = null;
            appContext.integrationContext.integrationPassword = null;
            appContext.integrationContext.integrationUserId = null;
        }
        catch (error)
        {
            console.error('Error:', error);
            return
        }

        console.log("Removing integration_credentials.json");

        fs.stat('./integration_credentials.json', function (err, stats)
        {
            console.log(stats);

            if (err) return console.error(err);

            fs.unlink('./integration_credentials.json', function(err)
            {
                if (err) return console.log(err);

                console.log('File deleted successfully');
            });
        });

        console.log("Removing integration_state.json");

        fs.stat('./integration_state.json', function (err, stats)
        {
            console.log(stats);

            if (err) return console.error(err);

            fs.unlink('./integration_state.json', function(err)
            {
                if (err) return console.log(err);

                console.log('File deleted successfully');
            });
        });
    });

    ipcMain.on("update-timestamp", (event, timestampMs) => {
        console.log("Update timestamp: ", timestampMs);

        let pushDataContext = appContext.integrationContext.deviceAgentPushDataContext;
        if (pushDataContext && pushDataContext.connected)
        {
            pushDataContext.pushDataInfo.timestampMs = timestampMs;

            pushDataContext.deviceAgent.pushData(pushDataContext.pushDataInfo);

            if (pushDataContext.frameCount > 50)
            {
                if (!pushDataContext.pdeFromEngine)
                    pushDataContext.deviceAgent.pushPluginDiagnosticEvent("error", "Caption", "PDE from Device Agent");
                else
                    pushDataContext.engine.pushPluginDiagnosticEvent("error", "Caption", "PDE from Engine");

                pushDataContext.pdeFromEngine = !pushDataContext.pdeFromEngine;
                pushDataContext.frameCount = 0;
            }

            pushDataContext.frameCount++;
        }
    });
}

app.whenReady().then(() => {
    initialize().then(() => {
        initializeNotifications();
        appContext.window = createWindow();
    });

}).catch((exception) => {
    console.log(exception);
});
