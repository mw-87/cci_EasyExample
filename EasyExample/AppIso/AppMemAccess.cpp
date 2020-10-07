/* ************************************************************************ */


/* ************************************************************************ */
#include "IsoDef.h"

#if defined(_LAY6_)

#include <iostream>
#include <sstream>
#include <string.h>
#include "Settings/settings.h"
#include "AppMemAccess.h"

#if defined(ESP_PLATFORM)
#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "AppMemAccess";

#endif // def ESP_PLATFORM

using namespace std;

#if defined(_MSC_VER )
#pragma warning(disable : 4996)
#endif // defined(_MSC_VER )
#if defined(linux) || defined(ESP_PLATFORM)
#define vswprintf_s swprintf
#define vsprintf_s snprintf
#define _strtoui64 strtoull
#define vswprintf_s swprintf
#define vsprintf_s snprintf
#define sprintf_s snprintf
#endif // defined(linux) || defined(ESP_PLATFORM)

#if defined(linux)
#define USE_L_FOR_64BIT
#elif defined(__MINGW_GCC_VERSION)
#define USE_LL_FOR_64BIT
#else // defined(linux), defined(__MINGW_GCC_VERSION)
#endif // defined(linux), defined(__MINGW_GCC_VERSION)
#if defined(ESP_PLATFORM)
   #define USE_LL_FOR_64BIT
#endif // def ESP_PLATFORM

static bool parseAuxEntry(char* entry, VT_AUXAPP_T* auxEntry);
static bool getKeyByID(iso_u16 wObjID_Fun, char* key, size_t size);
static bool getValue(const VT_AUXAPP_T& auxEntry, char* value, size_t size);

extern const int FIRST_AUX;
extern const int  LAST_AUX;

/* ****************   Object pool access   *********************************** */


#if defined(ESP_PLATFORM)
static esp_err_t register_vfs()
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret;
}
#endif // def ESP_PLATFORM


iso_u32 LoadPoolFromFile(const char * pcFilename, iso_u8 ** pPoolBuff)
{

	   /* load pool from file into RAM */
	   FILE *pFile;
	   iso_u32 u32PoolSize = 0;


#if defined(ESP_PLATFORM)
    if ( register_vfs() == ESP_OK ) {
#endif // def ESP_PLATFORM

	   if (*pPoolBuff)
	   {   /* free the pool data RAM */
		  free(*pPoolBuff);
		  *pPoolBuff = 0;
	   }

	   ESP_LOGI(TAG, "Opening file");
	   pFile = fopen(pcFilename, "rb");
	   if (pFile == NULL) {
	      ESP_LOGE(TAG, "Failed to open file for reading");
	   }
	   if (pFile)
	   {
		  fseek(pFile, 0L, SEEK_END);
		  u32PoolSize = static_cast<iso_u32>(ftell(pFile));
		  *pPoolBuff = reinterpret_cast<iso_u8*>(malloc(u32PoolSize));
		  fseek(pFile, 0L, SEEK_SET);
		  u32PoolSize = (iso_u32)fread(*pPoolBuff, sizeof(iso_u8), u32PoolSize, pFile);
		  fclose(pFile);
	   }

#if defined(ESP_PLATFORM)
       ESP_LOGI(TAG, "Pool size: %d, file: %s", u32PoolSize, pcFilename);
    }
#endif // def ESP_PLATFORM

   return u32PoolSize;
}



/* ****************   Auxiliary Assignments  *********************************** */
int getAuxAssignment(const char auxSection[], VT_AUXAPP_T asAuxAss[])
{
   size_t idxAux = 0U;
   for (int8_t idx = FIRST_AUX; idx <= LAST_AUX; idx++)
   {
	   char buffer[512];
       char key[64];
       getKeyByID(idx, key, sizeof(key));
       getString(auxSection, key, "", buffer, sizeof(buffer));
      VT_AUXAPP_T* auxEntry = &asAuxAss[idxAux];
      if (parseAuxEntry(buffer, auxEntry))
      {

          char value[64];

          getValue(*auxEntry, value, sizeof(value));
          iso_DebugPrint("getAuxAssignment: %d %s %s\n", idxAux, key, value);
          idxAux++;
      }
   }

   return (int)idxAux;
}


bool parseAuxEntry(char* entry, VT_AUXAPP_T* auxEntry)
{
    int wObjID_Fun;
    int wObjID_Input;
    int eAuxType;
    int wManuCode;
    int wModelIdentCode;
    int qPrefAssign;
    int bFuncAttribute;
    uint64_t name;
    int parameterCount = sscanf(entry,
#if defined(USE_L_FOR_64BIT)
        "%d=%d,%d,%d,%d,%d,%d,%lX", &wObjID_Fun,
#elif defined(USE_LL_FOR_64BIT)
        "%d=%d,%d,%d,%d,%d,%d,%llX", &wObjID_Fun,
#else // !defined(USE_L_FOR_64BIT)
        "%d=%d,%d,%d,%d,%d,%d,%I64X", &wObjID_Fun,
#endif //!defined(USE_L_FOR_64BIT)
        &wObjID_Input, &eAuxType, &wManuCode, &wModelIdentCode,
        &qPrefAssign, &bFuncAttribute, &name);
    if (parameterCount == 8)
    {
        auxEntry->wObjID_Fun = static_cast<iso_u16>(wObjID_Fun);              /* Object ID of auxiliary function */
        auxEntry->wObjID_Input = static_cast<iso_u16>(wObjID_Input);          /* Object ID of auxiliary input */
        auxEntry->eAuxType = static_cast<VTAUXTYP_e>(eAuxType);      /* Function/input type (without attribute bits)
                                                            - only because of downwards compatibility  */
        auxEntry->wManuCode = static_cast<iso_u16>(wManuCode);                /* Manufacturer Code of auxiliary input device */
        auxEntry->wModelIdentCode = static_cast<iso_u16>(wModelIdentCode);    /* Model identification code of aux input device */
        auxEntry->qPrefAssign = static_cast<iso_bool>(qPrefAssign);            /* This assignment shall used for preferred assignment */
        auxEntry->bFuncAttribute = static_cast<iso_u8>(bFuncAttribute);      /* Complete function attribute byte of auxiliary function */
        memcpy(&auxEntry->baAuxName[0], &name, 8);      /* ISO name of the auxiliary input device. The bytes must be set to 0xFF if not used. */
        return true;
    }

    return false;
}

void setAuxAssignment(const char section[], VT_AUXAPP_T asAuxAss[], iso_s16 iNumberOfAssigns)
{

	char key[64];
   // erase complete section
   for (int8_t idx = FIRST_AUX; idx <= LAST_AUX; idx++)
   {
       getKeyByID(idx, key, sizeof(key));
       eraseString(section, key);
   }


   char buffer[512];

   // write aux entries
   for (int8_t idx = 0; idx < iNumberOfAssigns; idx++)
   {
      VT_AUXAPP_T* auxEntry = &asAuxAss[idx];
      getKeyByID(asAuxAss->wObjID_Fun, key, sizeof(key));
      uint64_t name = 0;
      memcpy(&name, &auxEntry->baAuxName[0], 8);            /* ISO name of the auxiliary input device. The bytes must be set to 0xFF if not used. */
#if defined(USE_L_FOR_64BIT)
      sprintf_s(buffer, sizeof(buffer), "%d,%d,%d,%d,%d,%d,%lX",
#elif defined(USE_LL_FOR_64BIT)
      sprintf_s(buffer, sizeof(buffer), "%d,%d,%d,%d,%d,%d,%llX",
#else // !defined(USE_L_FOR_64BIT)
      sprintf_s(buffer, sizeof(buffer), "%d,%d,%d,%d,%d,%d,%I64X",
#endif //!defined(USE_L_FOR_64BIT)
         auxEntry->wObjID_Input, auxEntry->eAuxType, auxEntry->wManuCode, auxEntry->wModelIdentCode,
         auxEntry->qPrefAssign, auxEntry->bFuncAttribute, name);
      setString(section, key, buffer);
   }
}

void updateAuxAssignment(const char auxSection[], VT_AUXAPP_T* sAuxAss)
{
    if (sAuxAss->wObjID_Input != 0xFFFF)
    {
        char key[64];
        char value[64];
        getKeyByID(sAuxAss->wObjID_Fun, key, sizeof(key));
        getValue(*sAuxAss, value, sizeof(value));
        iso_DebugPrint("updateAuxAssignment add: %s %s\n", key, value);
        setString(auxSection, key, value);
    }
    else
    {
        iso_s16 auxCfHandle = IsoClGetCfHandleToName(ISO_CAN_VT, &sAuxAss->baAuxName);
        iso_u16 wModelIdentCode = 0;
        if (IsoReadAuxInputDevModIdentCode(auxCfHandle, &wModelIdentCode) == E_NO_ERR)
        {
            sAuxAss->wModelIdentCode = wModelIdentCode;
        }

        char key[64];
        getKeyByID(sAuxAss->wObjID_Fun, key, sizeof(key));
        iso_DebugPrint("updateAuxAssignment remove: %s\n", key);
        eraseString(auxSection, key);
    }
}

static bool getKeyByID(iso_u16 wObjID_Fun, char* key, size_t size)
{
    sprintf_s(key, size, "AUX-%d", wObjID_Fun);
    return true;
}

static bool getValue(const VT_AUXAPP_T& auxEntry, char* value, size_t size)
{
    uint64_t name = 0;
    memcpy(&name, &auxEntry.baAuxName[0], 8);            /* ISO name of the auxiliary input device. The bytes must be set to 0xFF if not used. */
#if defined(USE_L_FOR_64BIT)
    sprintf_s(value, size, "%d,%d,%d,%d,%d,%d,%lX",
#elif defined(USE_LL_FOR_64BIT)
    sprintf_s(value, size, "%d,%d,%d,%d,%d,%d,%llX",
#else // !defined(USE_L_FOR_64BIT)
    sprintf_s(value, size, "%d,%d,%d,%d,%d,%d,%I64X",
#endif // !defined(USE_L_FOR_64BIT)
        auxEntry.wObjID_Input, auxEntry.eAuxType, auxEntry.wManuCode, auxEntry.wModelIdentCode,
        auxEntry.qPrefAssign, auxEntry.bFuncAttribute, name);
    return true;
}

/* ************************************************************************ */
#endif /* defined(_LAY6_) */
/* ************************************************************************ */
