/*
 * Copyright (C) 2020  Joshua Minter
 *
 * This file is part of ReplaySorcery.
 *
 * ReplaySorcery is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ReplaySorcery is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ReplaySorcery.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "util/buffer.h"
#include "util/log.h"
#include "util/memory.h"
#include "util/path.h"
#include "util/string.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define CONFIG_NAME "replay-sorcery.conf"

typedef struct ConfigParam {
   const char *key;
   size_t offset;
   void (*set)(void *param, const char *value);
   const char *def;
} ConfigParam;

#define CONFIG_PARAM(key, set, def)                                                      \
   { #key, offsetof(RSConfig, key), set, def }

static void configInt(void *param, const char *value) {
   int *num = param;
   char *end;
   long ret = strtol(value, &end, 10);
   if (*end != '\0' || ret < INT_MIN || ret > INT_MAX) {
      rsError("Config value '%s' is not a valid integer", value);
   }
   *num = (int)ret;
}

static void configString(void *param, const char *value) {
   char **str = param;
   rsMemoryDestroy(*str);
   size_t size = strlen(value) + 1;
   *str = rsMemoryCreate(size);
   memcpy(*str, value, size);
}

static const ConfigParam configParams[] = {
    CONFIG_PARAM(offsetX, configInt, "0"),
    CONFIG_PARAM(offsetY, configInt, "0"),
    CONFIG_PARAM(width, configInt, "1920"),
    CONFIG_PARAM(height, configInt, "1080"),
    CONFIG_PARAM(framerate, configInt, "30"),
    CONFIG_PARAM(duration, configInt, "30"),
    CONFIG_PARAM(compressQuality, configInt, "70"),
    CONFIG_PARAM(outputFile, configString, "~/Videos/ReplaySorcery_%F_%H-%M-%S.mp4")};

#define CONFIG_PARAMS_SIZE (sizeof(configParams) / sizeof(ConfigParam))

static void configSet(RSConfig *config, const char *key, const char *value) {
   rsLog("Setting config key '%s' to '%s'...", key, value);
   for (size_t i = 0; i < CONFIG_PARAMS_SIZE; ++i) {
      if (strcmp(configParams[i].key, key) == 0) {
         void *param = (char *)config + configParams[i].offset;
         configParams[i].set(param, value);
         return;
      }
   }
   rsLog("Config key '%s' does not exist", key);
}

static void configLoadLine(RSConfig *config, char *line) {
   line = rsStringTrimEnd(rsStringTrimStart(line));
   // Remove comment
   char *comment = strchr(line, '#');
   if (comment != NULL) {
      *comment = '\0';
   }
   if (*line == '\0') {
      return;
   }

   char *eq = strchr(line, '=');
   if (eq == NULL) {
      rsError(
          "Failed to load config line, format is <key> = <value> # <optional comment>");
   }
   *eq = '\0';
   char *key = rsStringTrimEnd(line);
   char *value = rsStringTrimStart(eq + 1);
   configSet(config, key, value);
}

static void configLoadFile(RSConfig *config, const char *dir) {
   RSBuffer path;
   rsBufferCreate(&path);
   rsPathAppend(&path, dir);
   rsPathAppend(&path, CONFIG_NAME);

   rsLog("Loading config file '%s'...", path.data);
   FILE *file = fopen(path.data, "r");
   rsBufferDestroy(&path);
   if (file == NULL) {
      rsLog("Failed to open config file: %s", strerror(errno));
      return;
   }

   // Read the whole file
   fseek(file, 0, SEEK_END);
   size_t size = (size_t)ftell(file);
   fseek(file, 0, SEEK_SET);
   char buffer[size + 1];
   if (fread(buffer, size, 1, file) <= 0) {
      rsError("Failed to read from config file");
   }
   fclose(file);
   buffer[size] = '\0';

   char *ptr = buffer;
   char *line;
   while ((line = rsStringSplit(&ptr, '\n')) != NULL) {
      configLoadLine(config, line);
   }
}

void rsConfigLoad(RSConfig *config) {
   // Load defaults
   rsLog("Loading defaults...");
   rsMemoryClear(config, sizeof(RSConfig));
   for (size_t i = 0; i < CONFIG_PARAMS_SIZE; ++i) {
      configSet(config, configParams[i].key, configParams[i].def);
   }

   // Global configs
   const char *dirs = getenv("XDG_CONFIG_DIRS");
   if (dirs == NULL) {
      dirs = "/etc/xdg";
   }
   // Make a copy so we can modify it
   size_t size = strlen(dirs) + 1;
   char buffer[size];
   memcpy(buffer, dirs, size);
   char *ptr = buffer;
   char *dir;
   while ((dir = rsStringSplit(&ptr, ':')) != NULL) {
      configLoadFile(config, dir);
   }

   // User config
   const char *home = getenv("XDG_CONFIG_HOME");
   if (home == NULL) {
      home = "~/.config";
   }
   configLoadFile(config, home);

   // Relative config
   configLoadFile(config, ".");

   if (config->width % 8 != 0 || config->height % 8 != 0) {
      rsError("Only frame sizes divisable by 2 are supported");
   }
}

void rsConfigDestroy(RSConfig *config) {
   rsMemoryDestroy(config->outputFile);
   rsMemoryClear(config, sizeof(RSConfig));
}
