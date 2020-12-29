/*
 * Copyright (c) 2020 Huawei Technologies Co.,Ltd.
 *
 * openGauss is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 *
 * gaussdb_version.cpp
 *
 *   Disable some features by parsing configuration file.
 * This configuration file named 'gaussdb.version' was generated by OM script.
 * We parse this configuration file and initialize global private variable 'disabled_features_flag',
 * so that we can call is_feature_disabled() function to check if one feature was disabed.
 *
 * IDENTIFICATION
 *      src/gausskernel/process/postmaster/gaussdb_version.cpp
 *
 * -------------------------------------------------------------------------
 */
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include "securec.h"
#include "securec_check.h"
#include "utils/elog.h"
#include "utils/pg_crc.h"
#include "utils/palloc.h"
#include "libpq/sha2.h"
#include "alarm/alarm.h"
#include "libpq/pqformat.h"
#include "gaussdb_version.h"
#include "gssignal/gs_signal.h"
#include "postgres.h"
#include "knl/knl_variable.h"
#include "pgxc/pgxc.h"

extern THR_LOCAL char* application_name;

/* The maximum number of the gaussdb features. */
#define MAX_FEATURE_NUM 1024
/* The number of the sha256 count, it must be equal with the number of the product version. */
#define SHA256_DIGESTS_COUNT 2
/* The maximun length of the feature name. */
#define MAX_ALARM_FEATURE_NAME_LEN 64

/* The standard edition license name. */
#define STANDARD_EDITION_NAME "Standard Edition License"
/* The premium edition license name. */
#define PREMIUM_EDITION_NAME "Advanced Edition License"
/* The premium feature edition name. */
#define FEATURE_VECTOR_ENGINE_NAME "Feature-Vector Engine License"

/* Product version control file name. */
#define PRODUCT_VERSION_FILE "gaussdb.version"
/* License version control file name. */
#define LICENSE_VERSION_FILE "gaussdb.license"

/* Application OM. */
#define APPLICATION_OM "OM"

/* The feature report time interval. */
#define ALARM_REPORT_TIME_INTERVAL ((time_t)(24 * 60 * 60))

typedef struct {
    bool disabled_features_flag; /**
                                  * Set the default value, means that enable all feature by default.
                                  *  The disabled status is mandatory, user can not use the disabled features.
                                  */
    bool isReported;             /* Whether the alarm is reported. */
    time_t lastReportTime;       /* Save the alarm last reported time. */
    Alarm alarmPermissionDenied; /* Alarm item that used to report the permission denied alarm. */
} FeatureInfo;

typedef struct {
    const char* productFileName;              /* The default control file name. */
    uint32 licenseVersion;                    /* The version flag in the control file. */
    FeatureInfo featureInfo[MAX_FEATURE_NUM]; /* The feature information item. */
    bool isLoaded;                            /* Whether the feature control file is loaded. */
} LicenseControl;                             /* The data structure that used to store the control file information. */

/* The control file header information. */
typedef struct {
    unsigned int crc32_code;      /* Store the crc32 code of the data information. */
    unsigned int base64_data_len; /* Store the data length of the data information after the base64 decode. */
    unsigned int data_len;        /* Store the data length of the data information. */
} version_header;

/* The control file data information. */
typedef struct {
    unsigned int product_flag;               /* Store the product flag and the license flag(reversed). */
    unsigned int disabled_feature_len;       /* Store the number of the disabled features. */
    unsigned short disabled_feature_list[1]; /* Store the disabled feature id. */
} version_data;

/* The control file information. */
typedef struct {
    version_header* header; /* Store the control file header information. */
    version_data* data;     /* Store the control file data information. */
} gaussdb_version;

typedef struct {
    const char* featureName;
    const feature_name feature;
} feature_name_map;

/* Flag that whether the license module is initialized. */
static bool isInitialized = false;
/**
 * The data of this array will be filled in the package operation.
 *  And reset after the package operation.
 *  Please do not modify it.
 */
const char *sha256_digests[SHA256_DIGESTS_COUNT] = {NULL, NULL};
/* The product control file information. */
static LicenseControl versionControl = {PRODUCT_VERSION_FILE, PRODUCT_VERSION_UNKNOWN, {0}, false};
/* The license control file information. */
static LicenseControl licenseControl = {LICENSE_VERSION_FILE, 0, {0}, false};

/* The Premium edition license features. */
static const feature_name ADVANCED_EDITION[] = {
    EXPRESS_CLUSTER, CROSS_DC_COLLABORATION, ROW_LEVEL_SECURITY, TRANSPARENT_ENCRYPTION, PRIVATE_TABLE};
/* The map between the feature name and feature id. */
static const feature_name_map featureNameMap[] = {{"Multi Value Column", MULTI_VALUE_COLUMN},
    {"Json", JSON},
    {"Xml", XML},
    {"Data Storge Format", DATA_STORAGE_FORMAT},
    {"GTM Free", GTM_FREE},
    {"Double Live Disaster Recovery In The Same City", DOUBLE_LIVE_DISASTER_RECOVERY_IN_THE_SAME_CITY},
    {"Disaster Recovery In Two Places And Three Centers", DISASTER_RECOVERY_IN_TWO_PLACES_AND_THREE_CENTRES},
    {"GPU Acceleration In Multidimensional Collision Analysis",
        GPU_ACCELERATION_IN_MULTIDIMENSIONAL_COLLISION_ANALYSIS},
    {"Full Text Index", FULL_TEXT_INDEX},
    {"Extension Connector", EXTENSION_CONNECTOR},
    {"Sql On HDFS", SQL_ON_HDFS},
    {"Sql On OBS", SQL_ON_OBS},
    {"Express Cluster", EXPRESS_CLUSTER},
    {"Cross DC Collaboration", CROSS_DC_COLLABORATION},
    {"Graph Computing Engine", GRAPH_COMPUTING_ENGINE},
    {"Sequential Data Engine", SEQUENTIAL_DATA_ENGINE},
    {"PostGis Docking", POSTGIS_DOCKING},
    {"HA Single Primary Multi Standby", HA_SINGLE_PRIMARY_MULTI_STANDBY},
    {"Row Level Security", ROW_LEVEL_SECURITY},
    {"Transparent Encryption", TRANSPARENT_ENCRYPTION},
    {"Private Table", PRIVATE_TABLE}};
/* These feature will report the alarm on the DN, we will not limit it. */
static const feature_name EXCEPT_FEATURES[] = {EXTENSION_CONNECTOR};
/* Skip the application name. */
static const char* APPLICATION_WHITE_LIST[] = {APPLICATION_OM};

static int base64_decode(const unsigned char*, unsigned int, char*, unsigned int*);
extern "C" {
static int parse_gaussdb_version_file(gaussdb_version*, const char*);
}
static int sha256_encode(const unsigned char*, size_t, char*, size_t);
static int loadConrtolData(LicenseControl* control);
static void reportPermissionDeniedAlarm(feature_name feature);
static void reportResumeAlarm();
static const char* getLicenseEditionName(feature_name feature);
static const char* getFeatureName(feature_name feature, char* alarmFeatureName, size_t alarmFeatureNameLen);

/**
 * @brief: Get the product version number.
 *
 * @return uint32: Return the product version number.
 */
uint32 get_product_version()
{
    if (!versionControl.isLoaded) {
        return PRODUCT_VERSION_UNKNOWN;
    }

    return versionControl.licenseVersion;
}

/**
 * @brief: Decode the base64 data.
 *
 * @param [in] indata: The input data.
 * @param [in] inlen: The size of the input data.
 * @param [out] outdata: The output data, you should give enough space.
 * @param [in] outlen: The size of output data.
 *
 * @return 0: Success
 * @return -1: Failure
 */
int base64_decode(const unsigned char* indata, unsigned int inlen, char* outdata, unsigned int* outlen)
{
    static const unsigned char base64_suffix_map[256] = {255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        253,
        255,
        255,
        253,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        253,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        62,
        255,
        255,
        255,
        63,
        52,
        53,
        54,
        55,
        56,
        57,
        58,
        59,
        60,
        61,
        255,
        255,
        255,
        254,
        255,
        255,
        255,
        0,
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
        9,
        10,
        11,
        12,
        13,
        14,
        15,
        16,
        17,
        18,
        19,
        20,
        21,
        22,
        23,
        24,
        25,
        255,
        255,
        255,
        255,
        255,
        255,
        26,
        27,
        28,
        29,
        30,
        31,
        32,
        33,
        34,
        35,
        36,
        37,
        38,
        39,
        40,
        41,
        42,
        43,
        44,
        45,
        46,
        47,
        48,
        49,
        50,
        51,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255,
        255};
    unsigned int t = 0;
    unsigned int x = 0;
    unsigned int y = 0;
    unsigned int i = 0;
    unsigned char c = 0;
    int g = 3;

    if (indata == NULL || inlen <= 0 || outdata == NULL || outlen == NULL || inlen % 4 != 0) {
        ereport(WARNING, (errmsg("input arguments are error, please check those.")));
        goto error;
    }

    while (x < inlen && indata[x] != 0) {
        c = base64_suffix_map[indata[x++]];
        if (c == 255) {
            break;
        }
        if (c == 253) {
            continue;
        }
        if (c == 254) {
            c = 0;
            g--;
        }
        t = (t << 6) | c;
        if (++y == 4) {
            outdata[i++] = (unsigned char)((t >> 16) & 0xff);
            if (g > 1) {
                outdata[i++] = (unsigned char)((t >> 8) & 0xff);
            }
            if (g > 2) {
                outdata[i++] = (unsigned char)(t & 0xff);
            }
            y = t = 0;
        }
    }

    *outlen = i;
    return 0;

error:
    ereport(WARNING, (errmsg("failed to decode base64 for configuration file.")));
    return -1;
}

/**
 * @brief Get the sha256 digest.
 *
 * @param [in] indata: The input data.
 * @param [in] inlen: The size of the input data.
 * @param [out] outdata: The output data.
 * @param [in] outlen: The size of the output data.
 *
 * @return 0: Success
 * @return -1: Failure
 */
int sha256_encode(const unsigned char* indata, size_t inlen, char* outdata, size_t outlen)
{
    SHA256_CTX2 sha256_ctx;
    unsigned char buf[SHA256_DIGEST_LENGTH] = {0};
    errno_t rc;

    if (outlen < SHA256_DIGEST_STRING_LENGTH)
        return -1;

    SHA256_Init2(&sha256_ctx);
    SHA256_Update2(&sha256_ctx, indata, inlen);
    SHA256_Final2(buf, &sha256_ctx);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        rc = sprintf_s(outdata + i * 2, outlen - i * 2, "%02x", buf[i]);
        if (rc == -1) {
            ereport(WARNING, (errmsg("failed to get the sha256 digest for configuration file.")));
            return -1;
        }
    }

    outdata[SHA256_DIGEST_STRING_LENGTH - 1] = '\0';
    return 0;
}

/**
 * @beief: Get the License Edition by the feature id.
 *
 * @param feature: The feature id.
 *
 * @return: Return the License Edition name. If the feature is not in the premium edition and the premium feature
 * edition, Return NULL.
 */
const char* getLicenseEditionName(feature_name feature)
{
    for (size_t i = 0; i < sizeof(ADVANCED_EDITION) / sizeof(feature_name); i++) {
        if (ADVANCED_EDITION[i] == feature) {
            return PREMIUM_EDITION_NAME;
        }
    }

    if (feature == GPU_ACCELERATION_IN_MULTIDIMENSIONAL_COLLISION_ANALYSIS) {
        return FEATURE_VECTOR_ENGINE_NAME;
    }

    /* Other will be treated as a standard feature. */
    if (feature < FEATURE_NAME_MAX_VALUE) {
        return STANDARD_EDITION_NAME;
    }

    return NULL;
}

/**
 * @brief: Get the feature name by the feature id.
 *
 * @param feature: The input feature id.
 * @param alarmFeatureName: Store the feature name with alarm type.
 * @param alarmFeatureNameLen: The feature name buffer length.
 *
 * @return: Return the specified feature name, if feature id is illegal, return "Invalid Feature Name".
 */
const char* getFeatureName(feature_name feature, char* alarmFeatureName, size_t alarmFeatureNameLen)
{
    errno_t ret = 0;
    const char* featureName = NULL;

    /* Get the feature name. */
    for (unsigned long long i = 0; i < sizeof(featureNameMap) / sizeof(feature_name_map); i++) {
        if (featureNameMap[i].feature == feature) {
            featureName = featureNameMap[i].featureName;
            break;
        }
    }

    /* If the feature name does not exist, return the default name. */
    if (featureName == NULL) {
        featureName = "Unknown Feature Name";
    }

    if (alarmFeatureName != NULL && alarmFeatureNameLen > 0) {
        ret = strncpy_s(alarmFeatureName, alarmFeatureNameLen, featureName, strlen(featureName));
        securec_check_c(ret, "", "");

        /* Replace the feature name to alarm type. */
        for (size_t i = 0; i < alarmFeatureNameLen; i++) {
            if (alarmFeatureName[i] == ' ') {
                alarmFeatureName[i] = '_';
            }
        }
    }

    return featureName;
}

/**
 * @brief Check the report condition.
 *  The EC feature will report on the random DN instance.
 *  The other feature will report on the Coordinator instance with external connection.
 */
bool checkAlarmReportCondition(feature_name feature)
{
    /* If the application name is in the white list, we will not report the alarm. */
    if (u_sess->attr.attr_common.application_name != NULL) {
        for (size_t i = 0; i < lengthof(APPLICATION_WHITE_LIST); i++) {
            if (strcmp(u_sess->attr.attr_common.application_name, APPLICATION_WHITE_LIST[i]) == 0) {
                return false;
            }
        }
    }

    /* Only the coordinator which was connected by external can send alram. */
    if (IS_PGXC_COORDINATOR && !IsConnFromCoord()) {
        return true;
    }

    /* The DataNode will only report the EC feature. */
    for (size_t i = 0; i < sizeof(EXCEPT_FEATURES) / sizeof(feature_name); i++) {
        if (EXCEPT_FEATURES[i] == feature && IS_PGXC_DATANODE) {
            return true;
        }
    }

    return false;
}

/**
 * @brief: Report an alarm that the target feature was not authorized to use.
 *
 * @param [in] feature: The feature id.
 *
 * @return: void
 */
void reportPermissionDeniedAlarm(feature_name feature)
{
    AlarmAdditionalParam tempAdditionalParam;
    const char* edition = NULL;
    const char* featureName = NULL;
    char alarmFeatureName[MAX_ALARM_FEATURE_NAME_LEN] = {0};

    /* Check the alarm report condition. */
    if (!checkAlarmReportCondition(feature)) {
        return;
    }

    /* We don't alarm the feature except the premium edition and the premium feature edition. */
    edition = getLicenseEditionName(feature);
    if (edition == NULL) {
        return;
    }
    /* Get the feature name. */
    featureName = getFeatureName(feature, alarmFeatureName, sizeof(alarmFeatureName));

    /**
     * We will not report the alarm. Because the previous alarm has been reported, but the time interval for
     *  re-reporting has not yet been reached.
     */
    if (time(NULL) - ALARM_REPORT_TIME_INTERVAL <= licenseControl.featureInfo[feature].lastReportTime &&
        licenseControl.featureInfo[feature].isReported) {
        return;
    }

    /* fill the alarm message */
    WriteAlarmAdditionalInfo(&tempAdditionalParam,
        "",
        alarmFeatureName,
        "",
        &licenseControl.featureInfo[feature].alarmPermissionDenied,
        ALM_AT_Fault,
        featureName,
        edition);

    /* report the alarm */
    AlarmReporter(&licenseControl.featureInfo[feature].alarmPermissionDenied, ALM_AT_Fault, &tempAdditionalParam);

    /* Set the alarm is reported. */
    licenseControl.featureInfo[feature].isReported = true;
    /* Save the reported time. */
    licenseControl.featureInfo[feature].lastReportTime = time(NULL);

    ereport(LOG,
        (errmsg(
            "Report an alarm that the feature \"%s\" was not authorized to use on the \"%s\".", featureName, edition)));
}

/**
 * @brief: Report some resume alarm for the reported features that these features was authorized to use now.
 *
 * @return: void
 */
void reportResumeAlarm()
{
    for (int i = 0; i < FEATURE_NAME_MAX_VALUE; i++) {
        char alarmFeatureName[MAX_ALARM_FEATURE_NAME_LEN] = {0};

        /**
         * Report the resumed alarm:
         * 1. Now the feature is enabled.
         */
        if (licenseControl.featureInfo[i].disabled_features_flag == false) {
            AlarmAdditionalParam tempAdditionalParam;
            const char* edition = NULL;
            const char* featureName = NULL;

            /* We don't alarm the feature except the premium edition and the premium feature edition. */
            edition = getLicenseEditionName((feature_name)i);
            if (edition == NULL) {
                continue;
            }
            /* Get the feature name. */
            featureName = getFeatureName((feature_name)i, alarmFeatureName, sizeof(alarmFeatureName));

            /* fill the alarm message */
            WriteAlarmAdditionalInfo(&tempAdditionalParam,
                "",
                alarmFeatureName,
                "",
                &licenseControl.featureInfo[i].alarmPermissionDenied,
                ALM_AT_Resume,
                featureName,
                edition);
            /* report the alarm */
            AlarmReporter(&licenseControl.featureInfo[i].alarmPermissionDenied, ALM_AT_Resume, &tempAdditionalParam);

            ereport(LOG,
                (errmsg("Report an resume alarm that the feature \"%s\" has been authorized to use on the %s now.",
                    featureName,
                    edition)));

            /* Reset the reported time. */
            licenseControl.featureInfo[i].lastReportTime = 0;
        }
    }
}

/**
 * @brief: Check whether the specified feature is disabled.
 *
 * @param [in] feature: The specified feature.
 *
 * @return true: The feature is disabled.
 * @return false: The feature is enabled.
 */
bool is_feature_disabled(feature_name feature)
{
    /* If the feature is disabled by the product control file. */
    if (versionControl.featureInfo[feature].disabled_features_flag)
        return true;

    /* The liccense control does not support the GaussDB 300 product. */
    if (versionControl.licenseVersion == PRODUCT_VERSION_GAUSSDB300) {
        return false;
    }

    /**
     * We will report an alarm in the following situations:
     * 1. The feature is disabled by the license control file.
     * 2. The license control file is not loaded.
     * 3. The alarm module and the license alarm item must be loaded.
     */
    if (isInitialized && (licenseControl.featureInfo[feature].disabled_features_flag || !licenseControl.isLoaded)) {
        /* Report the permission alarm. */
        reportPermissionDeniedAlarm(feature);
    }

    return false;
}

/**
 * @brief: Get the feature control information from the file, and store these config into config data structure.
 *
 * @param [inout] control: The input feature control data structure.
 *
 * @return 0: Get the feature control information from the file successfully.
 * @return -1: Failed to parse the config file or
 */
int loadConrtolData(LicenseControl* control)
{
    gaussdb_version version;
    unsigned short* list = NULL;
    uint32 len;
    int ret = 0;
    uint32 i = 0;

    do {
        /* Initialize the feature information. */
        for (i = 0; i < FEATURE_NAME_MAX_VALUE; i++) {
            /* First, set the flage to false. */
            control->featureInfo[i].disabled_features_flag = false;

            /* Initialize one time. */
            if (!isInitialized) {
                /* Initialize the alarm item. */
                AlarmItemInitialize(&control->featureInfo[i].alarmPermissionDenied,
                    ALM_AI_FeaturePermissionDenied,
                    ALM_AS_Normal,
                    NULL);
            }
        }

        /* Parse the gaussdb version file. */
        version.data = NULL;
        version.header = NULL;
        ret = parse_gaussdb_version_file(&version, control->productFileName);
        if (ret != 0) {
            /* Set the license control */
            control->isLoaded = control->isLoaded ? true : false;

            ereport(WARNING, (errmsg("failed to parse feature control file: %s.", control->productFileName)));
            break;
        }

        /* Store the feature control information. */
        control->licenseVersion = version.data->product_flag;

        /* Set the features flag. */
        list = version.data->disabled_feature_list;
        len = version.data->disabled_feature_len;
        for (i = 0; i < len; i++) {
            /* If it is in the disabled list, set it to true. */
            if (list[i] < MAX_FEATURE_NUM && list[i] < FEATURE_NAME_MAX_VALUE) {
                control->featureInfo[list[i]].disabled_features_flag = true;
            }
        }

        /* Set the license control */
        control->isLoaded = true;
    } while (0);

    if (version.data != NULL)
        pfree(version.data);
    if (version.header != NULL)
        pfree(version.header);

    return ret;
}

/**
 * @brief: Parse "gaussdb.version" and the "gaussdb.license" file, this file is provided by OM script.
 *
 * @param [out] version_info: The pointer of the gaussdb_version data structure.
 * @param [in] filename: The control file information data structure.
 *
 * @note: The caller must free elements of this structure when you need not to use this variable.
 *
 * @return 0: Success
 * @return -1: Failure
 */
int parse_gaussdb_version_file(gaussdb_version* version_info, const char* filename)
{
    struct stat file_info;
    unsigned int crc32 = 0;
    off_t file_size;
    char buff[1024] = {0};
    char sha256_digest[SHA256_DIGEST_STRING_LENGTH] = {0};
    bool sha256_found_flag = false;
    int sha256_null_count = 0;
    unsigned int outlen = 0;
    int rc;
    int fd = -1;
    unsigned char* file_content = NULL;
    version_header* header = NULL;
    version_data* data = NULL;
    errno_t errorno = EOK;
    errorno = memset_s(&file_info, sizeof(struct stat), 0, sizeof(struct stat));
    securec_check(errorno, "\0", "\0");

    /* get configuration file informations */
    char filepath[MAXPGPATH] = {0};
    char* gausshome = gs_getenv_r("GAUSSHOME");
    /* first, we will get GAUSSHOME path */
    if (gausshome != NULL) {
        check_backend_env(gausshome);
        rc = sprintf_s(filepath, sizeof(filepath), "%s/bin/%s", gausshome, filename);
        if (rc == -1) {
            ereport(WARNING, (errmsg("failed to call secure function.")));
            goto error;
        }
    } else {
        ereport(WARNING, (errmsg("not found GAUSSHOME enviroment variable.")));
        /* try to find this file in current working directory */
        rc = sprintf_s(filepath, sizeof(filepath), "./%s", filename);
        if (rc == -1) {
            ereport(WARNING, (errmsg("failed to call secure function.")));
            goto error;
        }
    }

    fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        ereport(WARNING,
            (errmsg("failed to open feature control file, please check whether it exists: FileName=%s,"
                    " Errno=%d, Errmessage=%s.",
                filename,
                errno,
                gs_strerror(errno))));
        goto error;
    }

    if (fstat(fd, &file_info) < 0) {
        goto error;
    }

    /* get the content of configuration file */
    file_size = file_info.st_size;
    if (file_size <= 0) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("file_size is invalid:[%ld]", file_size)));
    }
    file_content = (unsigned char*)palloc(file_size);
    if (file_content == NULL) {
        ereport(WARNING, (errmsg("can not palloc memory.")));
        goto error;
    }
    if (read(fd, file_content, file_size) != file_size) {
        goto error;
    }

    /* check sha256 code */
    if (sha256_encode(file_content, file_size, sha256_digest, sizeof(sha256_digest)) == -1) {
        ereport(WARNING, (errmsg("can not get sha256 digest.")));
        goto error;
    }

    for (int i = 0; i < SHA256_DIGESTS_COUNT; i++) {
        if (sha256_digests[i] == NULL) {
            sha256_null_count++;
            continue;
        }

        if (strcmp(sha256_digests[i], sha256_digest) == 0) {
            sha256_found_flag = true;
            break;
        }
    }

    /**
     * this gaussdb.version file exists, but sha256 digests are not consistent,
     * we will give a fatal report to user.
     */
    if (strcmp(filename, PRODUCT_VERSION_FILE) == 0) {
        if (!sha256_found_flag && sha256_null_count != SHA256_DIGESTS_COUNT) {
            close(fd);
            ereport(
                FATAL, (errmsg("\"%s\" configuration file is not official, please check it.", PRODUCT_VERSION_FILE)));
        }

        ereport(LOG, (errmsg("sha256 from \"%s\" is %s.", PRODUCT_VERSION_FILE, sha256_digest)));
    }

    /* deserialization from configuration file */
    header = (version_header*)palloc(sizeof(version_header));
    if (memcpy_s(header, sizeof(version_header), file_content, sizeof(version_header)) != 0) {
        goto error;
    }

    /* check for CRC32 equality */
    if (sizeof(version_header) + header->base64_data_len <= (size_t)file_size) {
        COMP_CRC32(crc32, file_content + sizeof(version_header), header->base64_data_len);
    }
    if (header->crc32_code != crc32) {
        ereport(WARNING, (errmsg("The CRC32 check result of file is inconsistent.")));
        goto error;
    }

    rc = base64_decode(file_content + sizeof(version_header), header->base64_data_len, buff, &outlen);
    if (rc < 0) {
        goto error;
    }

    if (outlen != header->data_len) {
        ereport(WARNING,
            (errmsg("failed to check data length, outlen is %u, data_len is %u."
                    " please check the configuration file.",
                outlen,
                header->data_len)));
        goto error;
    }

    data = (version_data*)palloc(header->data_len);
    if (memcpy_s(data, header->data_len, buff, header->data_len) != 0) {
        goto error;
    }

    version_info->header = header;
    version_info->data = data;
    pfree(file_content);
    close(fd);
    return 0;

error:
    if (fd >= 0) {
        close(fd);
    }
    if (file_content != NULL) {
        pfree(file_content);
    }
    if (data != NULL) {
        pfree(data);
    }
    if (header != NULL) {
        pfree(header);
    }

    return -1;
}

/**
 * @brief: Receive the SIG_RELOAD_LICENSE signal, reload the license control information from config file.
 *
 * @param [in] sig: The signal id.
 */
void signalReloadLicenseHandler(int sig)
{
    /* Only GaussDB200 support the license resume operation. */
    if (get_product_version() != PRODUCT_VERSION_GAUSSDB200) {
        return;
    }

    ereport(LOG, (errmsg("Reloading the license control file.")));

    if (loadConrtolData(&licenseControl) != 0) {
        ereport(WARNING,
            (errmsg("Failed to reload the license control file, so the license information will not be changed.")));
        return;
    }

    ereport(LOG, (errmsg("Reload the license control file successfully.")));

    ereport(LOG, (errmsg("Resuming the alarm for the new enabled features.")));
    /**
     * We will resume an alarm in the following situation:
     * 1. The feature is enabled by the license control file
     *  flag is set.
     */
    reportResumeAlarm();
}

/**
 * @brief: Initialize the feature flag array, so that we can know if one feature is enabled by calling
 * is_feature_disabled().
 *
 * @return: void
 */
void initialize_feature_flags()
{
    MemoryContext oldcontext = MemoryContextSwitchTo(INSTANCE_GET_MEM_CXT_GROUP(MEMORY_CONTEXT_CBB));
    /* Initialize the alarm module. */
    AlarmEnvInitialize();

    /* Load the product version control file. */
    if (loadConrtolData(&versionControl) != 0) {
        ereport(WARNING,
            (errmsg("Failed to load the product control file, so gaussdb cannot distinguish product version.")));
        return;
    }

    /* Only GaussDB200 support the license operation. */
    if (get_product_version() != PRODUCT_VERSION_GAUSSDB200) {
        return;
    }

    /* Load the license contro file. */
    if (loadConrtolData(&licenseControl) != 0) {
        ereport(WARNING,
            (errmsg("Failed to load the license control file, so gaussdb cannot distinguish license version.")));
    }

    /**
     * We will resume an alarm in the following situation:
     * 1. The feature is enabled by the license control file
     *  flag is set.
     */
    reportResumeAlarm();

    /* Set the intialized flag. */
    isInitialized = true;
    (void)MemoryContextSwitchTo(oldcontext);
}
// end of file
