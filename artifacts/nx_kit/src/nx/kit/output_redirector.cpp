// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "output_redirector.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "ini_config.h"
#include "utils.h"

#if defined(_WIN32)
    #pragma warning(disable: 4996) //< MSVC: freopen() is unsafe.
#endif

#if defined(__ANDROID__)
    #include <jni.h>

    JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
    {
        JNIEnv* env = nullptr;

        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
            return JNI_ERR;

        // Call QtNative.activity().getApplicationContext().getFilesDir().getPath()

        jclass nativeActivityClass = env->FindClass("org/qtproject/qt/android/QtNative");
        jmethodID activityMethod = env->GetStaticMethodID(
            nativeActivityClass, "activity", "()Landroid/app/Activity;");
        jobject activityInstance = env->CallStaticObjectMethod(nativeActivityClass, activityMethod);

        jclass contextClass = env->FindClass("android/content/Context");
        jmethodID appContextMethod = env->GetMethodID(
            contextClass, "getApplicationContext", "()Landroid/content/Context;");
        jobject appContext = env->CallObjectMethod(activityInstance, appContextMethod);

        jmethodID filesDirMethod = env->GetMethodID(
            env->GetObjectClass(appContext), "getFilesDir", "()Ljava/io/File;");
        jobject fileObject = env->CallObjectMethod(appContext, filesDirMethod);

        jmethodID pathMethod = env->GetMethodID(
            env->GetObjectClass(fileObject), "getPath", "()Ljava/lang/String;");
        jstring pathObject = (jstring) env->CallObjectMethod(fileObject, pathMethod);

        const char* utf = env->GetStringUTFChars(pathObject, nullptr);
        std::string filesDir(utf);
        env->ReleaseStringUTFChars(pathObject, utf);

        filesDir += '/';
        nx::kit::IniConfig::setIniFilesDir(filesDir.c_str());
        nx::kit::OutputRedirector::getInstance();

        return JNI_VERSION_1_6;
    }
#else
    namespace nx {
    namespace kit {
    /** The redirection is performed by this static initialization. */
    const OutputRedirector& unused_OutputRedirector = nx::kit::OutputRedirector::getInstance();
    } // namespace kit
    } // namespace nx
#endif

namespace nx {
namespace kit {

static bool redirectOutput(FILE* stream, const char* streamName, const std::string& filename);

static bool fileExists(const std::string& filePath);

/*static*/ void OutputRedirector::ensureOutputRedirection()
{
    // Should be empty. The only purpose for it is to ensure that the linker will not optimize away
    // static initialization for this library.
}

const OutputRedirector& OutputRedirector::getInstance()
{
    static const OutputRedirector redirector;

    return redirector;
}

OutputRedirector::OutputRedirector(const char* overridingLogFilesDir /*= nullptr*/)
{
    #if !defined(NX_OUTPUT_REDIRECTOR_DISABLED)
        const std::string logFilesDir =
            overridingLogFilesDir ? overridingLogFilesDir : nx::kit::IniConfig::iniFilesDir();

        const std::string processName = nx::kit::utils::getProcessName();

        static const std::string kStdoutFilename = processName + "_stdout.log";
        static const std::string kStderrFilename = processName + "_stderr.log";

        if (fileExists(logFilesDir + kStdoutFilename))
            m_isStdoutRedirected = redirectOutput(stdout, "stdout", logFilesDir + kStdoutFilename);

        if (fileExists(logFilesDir + kStderrFilename))
            m_isStderrRedirected = redirectOutput(stderr, "stderr", logFilesDir + kStderrFilename);
    #endif
}

static bool redirectOutput(FILE* stream, const char* streamName, const std::string& filename)
{
    if (!freopen(filename.c_str(), "w", stream))
    {
        fprintf(stderr, "ERROR: Unable to perform redirection of %s to %s\n",
            streamName, filename.c_str());
        return false;
    }

    const std::string processName = nx::kit::utils::getProcessName();

    fprintf(stream, "%s of %s is redirected to this file (%s)\n",
        streamName,
        nx::kit::utils::toString(processName).c_str(),
        nx::kit::utils::toString(filename).c_str());

    return true;
}

static bool fileExists(const std::string& filePath)
{
    return static_cast<bool>(std::ifstream(filePath.c_str()));
}

} // namespace kit
} // namespace nx
