/*
  DePlayEnabler plugin by Alpakeno.
  Enable video debug play mode with the ability to change sd0 and ux0 folder path from configuration file.
  RegMgrGetKeyInt patch by SilicaAndPina.
  Plugin made with help from folks at the CBPS discord: https://discord.cbps.xyz
  Specially @Princess of TB, @Goddess of Sleeping and others folks too, @Queen Devbot for the plugin's name idea.
  Thank you all.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <vitasdk.h>
#include <taihen.h>

#define CONFIG_PATH "ur0:/tai/deplayenabler.txt"
#define CONFIG_CONT "USE=true\r\nSD0=uma0:/video/\r\nUX0=ux0:/video/"

#define DEFAULT_SD0_PATH "uma0:/video/"
#define DEFAULT_UX0_PATH "ux0:/video/"

#define N_INJECTS 8
static SceUID inject_uid[N_INJECTS];
static int n_uids = 0;

#define N_HOOKS 5
tai_hook_ref_t hook_ref[N_HOOKS];
static int hook_uid[N_HOOKS];

static char *devices[] = {
    "host0:",
    "imc0:",
    "sd0:",
    "uma0:",
    "ur0:",
    "ux0:",
    "xmc0:",
    "vd0:",
};

#define N_DEVICES (sizeof(devices) / sizeof(char **))

unsigned char browse[19] = {0x42, 0x00, 0x72, 0x00, 0x6F, 0x00, 0x77, 0x00, 0x73, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

char enabled[5] = "true", sd0_path[65] = DEFAULT_SD0_PATH, ux0_path[65] = DEFAULT_UX0_PATH;

char *getRoot(char *path) {
	static char root[7];
	sceClibStrncpy(root, path, 7);
	char *colon = sceClibStrrchr(root, ':') + 1;
	if (colon == NULL) return root;
	colon[0] = (char)0;
	return root;
}

void removeSpaces(char *str) {
	int count = 0;
	for (int i = 0; str[i]; i++)
		if (str[i] != ' ')
			str[count++] = str[i];
	str[count] = '\0';
}

int checkName(const char *token, const char *name) {
	return (sceClibStrncmp(token, name, sce_paf_private_strlen(name)) == 0);
}

char *getValue(char *token){
	char *cfgvalue = (char*)calloc(1, sizeof(token));
	cfgvalue = (sce_paf_private_strchr(token, '=')) + 1;
	return cfgvalue;
}

void saveConfig(const char *path) {
	SceUID fd;
	fd = sceIoOpen(path, SCE_O_CREAT | SCE_O_WRONLY, 6);
	if (fd >= 0) {
		sceIoWrite(fd, CONFIG_CONT, sce_paf_private_strlen(CONFIG_CONT));
		sceIoClose(fd);
	}
}

int loadConfig(const char *path) {
	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0) {
		sceIoClose(fd);
		saveConfig(path);
		return fd;
	}
	char conf_buff[256];
	sceClibMemset(conf_buff, 0, 256);	
	sceIoRead(fd, conf_buff, 256);
	sceIoClose(fd);
	char *token = sce_paf_private_strtok(conf_buff, "\r\n");
	char *token_trimed = token;
	while (token != NULL) {
		removeSpaces(token_trimed);
		if (sce_paf_private_strlen(token_trimed) > 0) {
			if (token_trimed[0] != '#') {
				if (checkName(token_trimed, "USE")) {
					sceClibSnprintf(enabled, sizeof(enabled), getValue(token));
				} else if (checkName(token_trimed, "SD0")) {
					sceClibSnprintf(sd0_path, sizeof(sd0_path), getValue(token));
				} else if (checkName(token_trimed, "UX0")) {
					sceClibSnprintf(ux0_path, sizeof(ux0_path), getValue(token));
				}	
			}
		}
		token = sce_paf_private_strtok(NULL, "\r\n");
		token_trimed = token;
	}
	int i = 0, found_sd0 = -1, found_ux0 = -1;
	for (i = 0; i < N_DEVICES; i++) {
		if (sceClibStrcmp(devices[i], getRoot((char*)sd0_path)) == 0) found_sd0 = 1;
		if (sceClibStrcmp(devices[i], getRoot((char*)ux0_path)) == 0) found_ux0 = 1;
	}
	if (found_sd0 < 1) sceClibSnprintf(sd0_path, sizeof(sd0_path), DEFAULT_SD0_PATH);
	if (found_ux0 < 1) sceClibSnprintf(ux0_path, sizeof(ux0_path), DEFAULT_UX0_PATH);
	return 0;
}

static int sceRegMgrGetKeyInt_patched(const char *category, const char *name, int *buf) {
	int ret = TAI_CONTINUE(int, hook_ref[2], category, name, buf);
	if (sceClibStrcmp(name, "debug_videoplayer") == 0) {
		*buf = 3;
		return 0;
	}
	return ret;
}
	
SceUID scePafMisc_B3B5DF38_patched(int *a1, char *path, int a2, int a3, int a4) {
	SceUID ret = TAI_CONTINUE(SceUID, hook_ref[1], a1, path, a2, a3, a4);
	if (sceClibStrcmp(path, "vs0:vsh/common/libvideoprofiler.suprx") == 0) {		
		uint32_t *ptr = (uint32_t*)*a1;
		SceUID modid = ptr[3];
		inject_uid[n_uids++] = taiInjectData(modid, 0, 0x1e810, getRoot((char*)sd0_path), 7);
	}
	return ret;
}

int sce_paf_private_strcmp_patched(const char *str1, const char *str2) {
	if (sceClibStrncmp(str2, "ux0:", 4) == 0) return 0;
	return TAI_CONTINUE(int, hook_ref[3], str1, str2);	
}

int sceSysmoduleLoadModuleInternalWithArg_patched(SceSysmoduleInternalModuleId id, SceSize args, void *argp, void *unk){
	int res = TAI_CONTINUE(int, hook_ref[0], id, args, argp, unk);
	if (res >= 0 && id == 0x80000008) {
		hook_uid[1] = taiHookFunctionImport(&hook_ref[1], "SceVideoPlayer", 0x3D643CE8, 0xB3B5DF38, scePafMisc_B3B5DF38_patched);
		hook_uid[3] = taiHookFunctionImport(&hook_ref[3], TAI_MAIN_MODULE, 0xA7D28DAE, 0x5CD08A47, sce_paf_private_strcmp_patched);
	}
	return res;
}

uint32_t text_addr, text_size, data_addr, data_size;

void hook_8102df10_patched() {
	uint32_t *ptr_sd0  = (uint32_t*)(data_addr + (0x81185150  - 0x81185000) + 4);
	uint32_t *ptr_ux0  = (uint32_t*)(data_addr + (0x81185150  - 0x81185000) + 0xC);
	*ptr_sd0 = (unsigned)sd0_path;
	*ptr_ux0 = (unsigned)ux0_path;
}

void _start() __attribute__ ((weak, alias("module_start")));

int module_start(SceSize args, void *argp) {

	tai_module_info_t tai_info;
	tai_info.size = sizeof(tai_info);

	if (taiGetModuleInfo("SceVideoPlayer", &tai_info) >= 0) {

		SceKernelModuleInfo mod_info;
		mod_info.size = sizeof(SceKernelModuleInfo);
		int ret = sceKernelGetModuleInfo(tai_info.modid, &mod_info);
		if (ret < 0) return SCE_KERNEL_START_FAILED;
		text_addr = (uint32_t) mod_info.segments[0].vaddr;
		text_size = (uint32_t) mod_info.segments[0].memsz;
		data_addr = (uint32_t) mod_info.segments[1].vaddr;
		data_size = (uint32_t) mod_info.segments[1].memsz;

		// load configuration
		loadConfig(CONFIG_PATH);

		// check plugin state
		if (sceClibStrncmp(enabled, "false", 5) == 0) return SCE_KERNEL_START_SUCCESS;

		// inject uma0, ur0, ux0 etc. root to sd0 library
		hook_uid[0] = taiHookFunctionImport(&hook_ref[0], TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0xC3C26339, sceSysmoduleLoadModuleInternalWithArg_patched);

		// enable debug mode
		hook_uid[2] = taiHookFunctionImport(&hook_ref[2], TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0x16DDF3DC, sceRegMgrGetKeyInt_patched);

		// redirect sd0 and ux0 path to uma0, ur0, ux0 etc.
		hook_uid[4] = taiHookFunctionOffset(&hook_ref[4], tai_info.modid, 0, 0x2df10, 1, hook_8102df10_patched);

		// disable refreshing db
		uint8_t disable_refresh_db[2] = {0x00, 0xf0};
		inject_uid[n_uids++] = taiInjectData(tai_info.modid, 0, 0x22ca0, &disable_refresh_db[0], sizeof(disable_refresh_db[0]));
		inject_uid[n_uids++] = taiInjectData(tai_info.modid, 0, 0x22c9d, &disable_refresh_db[1], sizeof(disable_refresh_db[1]));

		// change TestFolder to Browse
		inject_uid[n_uids++] = taiInjectData(tai_info.modid, 0, 0x1656c4, &browse, sizeof(browse));
		inject_uid[n_uids++] = taiInjectData(tai_info.modid, 0, 0x1657e4, "Browse", 10);

		// extra patch to prevent the deletion of unsupported files
		uint8_t value = 0x0D;
		inject_uid[n_uids++] = taiInjectData(tai_info.modid, 0, 0x22b6e, &value, sizeof(value));

	}

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp) {
	int i;
	for (i = 0; i < N_HOOKS; i++) {
	   	if (hook_uid[i] >= 0) taiHookRelease(hook_uid[i], hook_ref[i]);
	}
	i = 0;
	for (i = n_uids - 1; i >= 0; i++) {
	  if (inject_uid[i] >= 0) taiInjectRelease(inject_uid[i]);
	}
  return SCE_KERNEL_STOP_SUCCESS;
}
