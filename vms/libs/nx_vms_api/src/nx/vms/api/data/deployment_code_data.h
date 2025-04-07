// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

/**apidoc Version determines the size of the QR Code. */
NX_REFLECTION_ENUM_CLASS(QrCodeVersion,
//  This table gives the information about the maximum QR Code data capacity in bytes for the
//  given version and the given error correction level.
//  version             low    medium    medHigh   high
    v21x21 = 1, //       17       14       11        7
    v25x25,     //       32       26       20       14
    v29x29,     //       53       42       32       24
    v33x33,     //       78       62       46       34
    v37x37,     //      106       84       60       44
    v41x41,     //      134      106       74       58
    v45x45,     //      154      122       86       64
    v49x49,     //      192      152      108       84
    v53x53,     //      230      180      130       98
    v57x57,     //      271      213      151      119
    v61x61,     //      321      251      177      137
    v65x65,     //      367      287      203      155
    v69x69,     //      425      331      241      177
    v73x73,     //      458      362      258      194
    v77x77,     //      520      412      292      220
    v81x81,     //      586      450      322      250
    v85x85,     //      644      504      364      280
    v89x89,     //      718      560      394      310
    v93x93,     //      792      624      442      338
    v97x97,     //      858      666      482      382
    v101x101,   //      929      711      509      403
    v105x105,   //     1003      779      565      439
    v109x109,   //     1091      857      611      461
    v113x113,   //     1171      911      661      511
    v117x117,   //     1273      997      715      535
    v121x121,   //     1367     1059      751      593
    v125x125,   //     1465     1125      805      625
    v129x129,   //     1528     1190      868      658
    v133x133,   //     1628     1264      908      698
    v137x137,   //     1732     1370      982      742
    v141x141,   //     1840     1452     1030      790
    v145x145,   //     1952     1538     1112      842
    v149x149,   //     2068     1628     1168      898
    v153x153,   //     2188     1722     1228      958
    v157x157,   //     2303     1809     1283      983
    v161x161,   //     2431     1911     1351     1051
    v165x165,   //     2563     1989     1423     1093
    v169x169,   //     2699     2099     1499     1139
    v173x173,   //     2809     2099     1579     1219
    v177x177    //     2953     2331     1663     1273
);

/**%apidoc
 * Correction level. The higher it is, the less data can be encoded but the more resistant the
 * resulting QR Code is to a physical image corruption.
 */
NX_REFLECTION_ENUM_CLASS(QrCodeCorrectionLevel,
    low,
    medium,
    mediumHigh,
    high
);

/**%apidoc Image type. */
NX_REFLECTION_ENUM_CLASS(QrCodeImageType,
    bmp,
    png
);

/**%apidoc Qr Code request data. */
struct NX_VMS_API DeploymentCodeRequestData
{
    /**%apidoc:string
     * Can be obtained from "id" field via `GET /rest/v{1-}/servers` or be `this` to refer to the
     * current Server.
     * %example this
     */
    nx::Uuid serverId;

    /**%apidoc
     * QR Code version. Can be omitted, in this case the smallest possible version will be chosen
     * which can fit given text and correction level.
     * %example v85x85
     */
    std::optional<QrCodeVersion> version = std::nullopt;

    /**%apidoc
     * QR Code correction level.
     * %example low
     */
    QrCodeCorrectionLevel correctionLevel = QrCodeCorrectionLevel::medium;

    /**%apidoc
     * QR Code image type.
     * %example png
     */
    QrCodeImageType imageType = QrCodeImageType::png;

    /**%apidoc
     * QR Code image pixel size.
     * %example 10
     */
    int pixelSize = 10;
};

#define DeploymentCodeRequestData_Fields (serverId)(version)(correctionLevel)(imageType) \
    (pixelSize)

NX_REFLECTION_INSTRUMENT(DeploymentCodeRequestData, DeploymentCodeRequestData_Fields)

/**%apidoc QR Code response. */
struct NX_VMS_API DeploymentCodeData
{
    nx::Uuid serverId;

    /**%apidoc Deployment code usable for adding Server during the Deployment Code Site set-up. */
    std::string code;

    /**%apidoc Hardware code which can be optionally used when adding Server during the
     * Deployment Code Site set-up.
     */
    std::string hardwareCode;

    /**%apidoc[opt] Complete URL leading to the Cloud Portal QR Code setup section. */
    std::string url;

    /**%apidoc[opt] Base64-encoded QR Code image. */
    std::string imageBase64;
};

#define DeploymentCodeData_Fields (serverId)(code)(hardwareCode)(url)(imageBase64)
NX_REFLECTION_INSTRUMENT(DeploymentCodeData, DeploymentCodeData_Fields)

QN_FUSION_DECLARE_FUNCTIONS(DeploymentCodeRequestData, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(DeploymentCodeData, (json), NX_VMS_API)

using DeploymentCodeDataList = std::vector<DeploymentCodeData>;

} // namespace nx::vms::api
