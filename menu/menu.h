#ifndef _MENU_H_
#define _MENU_H_

#define SYSTEM_DIR		"pocketsnes"

#define ROM_LIST_FILENAME		"romlist.bin"
#define SRAM_FILE_EXT			"srm"
#define SAVESTATE_EXT			"sv"
#define MENU_OPTIONS_FILENAME		"pocketsnes_options"
#define MENU_OPTIONS_EXT		"opt"
#define DEFAULT_ROM_DIR_FILENAME	"romdir"
#define DEFAULT_ROM_DIR_EXT		"opt"

#define SAVESTATE_MODE_SAVE			0
#define SAVESTATE_MODE_LOAD			1
#define SAVESTATE_MODE_DELETE			2

#define POCKETSNES_VERSION			"version 2"

#define MENU_NORMAL_CPU_SPEED 			336
#define MENU_FAST_CPU_SPEED			336

enum  MENU_ENUM
{
	MENU_RETURN = 0,
#ifndef NO_ROM_BROWSER
	MENU_ROM_SELECT,	
#endif
	MENU_STATE,
	MENU_RESET_GAME,
	MENU_SAVE_SRAM,
	MENU_AUTO_SAVE_SRAM,
	MENU_SOUND_ON,
//	MENU_SOUND_VOL,
	MENU_SOUND_RATE,
	MENU_SOUND_STEREO,
	MENU_FRAMESKIP,
	MENU_FULLSCREEN,
	MENU_FPS,
	MENU_SOUND_SYNC,
//	MENU_CPU_SPEED,
	MENU_LOAD_GLOBAL_SETTINGS,
	MENU_SAVE_GLOBAL_SETTINGS,
	MENU_LOAD_CURRENT_SETTINGS,
	MENU_SAVE_CURRENT_SETTINGS,
	MENU_DELETE_CURRENT_SETTINGS,
	MENU_CREDITS,
	MENU_EXIT_APP,
	MENU_COUNT
};

enum SAVESTATE_MENU_ENUM
{
	SAVESTATE_MENU_LOAD = 0,
	SAVESTATE_MENU_SAVE,
	SAVESTATE_MENU_DELETE,
	SAVESTATE_MENU_RETURN,
	SAVESTATE_MENU_COUNT
};

enum SRAM_MENU_ENUM
{
	SRAM_MENU_LOAD = 0,
	SRAM_MENU_SAVE,
	SRAM_MENU_DELETE,
	SRAM_MENU_RETURN,
	SRAM_MENU_COUNT,
};

enum EVENT_TYPES
{
	EVENT_NONE = 0,
	EVENT_EXIT_APP,
	EVENT_LOAD_ROM,
	EVENT_RUN_ROM,
	EVENT_RESET_ROM,
};

enum MENU_MESSAGE_BOX_MODE
{
	MENU_MESSAGE_BOX_MODE_MSG = 0,
	MENU_MESSAGE_BOX_MODE_PAUSE,
	MENU_MESSAGE_BOX_MODE_YESNO,
};

//Graphic size definitions
#define MENU_TILE_WIDTH      64
#define MENU_TILE_HEIGHT     64
#define MENU_HEADER_WIDTH    320
#define MENU_HEADER_HEIGHT   48
#define HIGHLIGHT_BAR_WIDTH  320
#define HIGHLIGHT_BAR_HEIGHT 16

#define INP_BUTTON_MENU_SELECT			SAL_INPUT_A
#define INP_BUTTON_MENU_CANCEL			SAL_INPUT_B
#define INP_BUTTON_MENU_ENTER			SAL_INPUT_SELECT
#define INP_BUTTON_MENU_DELETE			SAL_INPUT_SELECT
#define INP_BUTTON_MENU_PREVIEW_SAVESTATE	SAL_INPUT_Y
#define INP_BUTTON_MENU_QUICKSAVE1		SAL_INPUT_R
#define INP_BUTTON_MENU_QUICKSAVE2		SAL_INPUT_SELECT
#define INP_BUTTON_MENU_QUICKLOAD1		SAL_INPUT_L
#define INP_BUTTON_MENU_QUICKLOAD2		SAL_INPUT_SELECT
#define INP_BUTTON_MENU_EXIT			SAL_INPUT_EXIT
#define MENU_TEXT_LOAD_SAVESTATE 		"Press A to load"
#define MENU_TEXT_OVERWRITE_SAVESTATE		"Press A to overwrite"
#define MENU_TEXT_DELETE_SAVESTATE 		"Press A to delete"
#define MENU_TEXT_PREVIEW_SAVESTATE 		"Press Y to preview"


struct MENU_OPTIONS
{
  unsigned int optionsVersion;
  unsigned int frameSkip;
  unsigned int soundEnabled;
  /* The following setting was 'transparency', which is now always enabled.
   * This setting word cannot be reused for any other purpose. It is not
   * guaranteed to be initialised to 0 in most installs of PocketSNES. */
  unsigned int Unused_20140603_1;
  unsigned int volume;
  unsigned int pad_config[32];
  unsigned int country;
  unsigned int showFps;
  unsigned int stereo;
  unsigned int fullScreen;
  unsigned int autoSaveSram;
  unsigned int cpuSpeed;
  unsigned int soundRate;
  unsigned int soundSync;
  unsigned int spare02;
  unsigned int spare03;
  unsigned int spare04;
  unsigned int spare05;
  unsigned int spare06;
  unsigned int spare07;
  unsigned int spare08;
  unsigned int spare09;

};

struct SAVE_STATE
{
  s8 filename[SAL_MAX_PATH];
  s8 fullFilename[SAL_MAX_PATH];
  u32 inUse;
};










typedef enum{
    MENU_TYPE_VOLUME,
    MENU_TYPE_BRIGHTNESS,
    MENU_TYPE_SAVE,
    MENU_TYPE_LOAD,
    MENU_TYPE_ASPECT_RATIO,
    MENU_TYPE_EXIT,
    MENU_TYPE_POWERDOWN,
    NB_MENU_TYPES,
} ENUM_MENU_TYPE;


///------ Definition of the different aspect ratios
#define ASPECT_RATIOS \
    X(ASPECT_RATIOS_TYPE_MANUAL, "MANUAL ZOOM") \
    X(ASPECT_RATIOS_TYPE_STRECHED, "STRECHED") \
    X(ASPECT_RATIOS_TYPE_CROPPED, "CROPPED") \
    X(ASPECT_RATIOS_TYPE_SCALED, "SCALED") \
    X(NB_ASPECT_RATIOS_TYPES, "")

////------ Enumeration of the different aspect ratios ------
#undef X
#define X(a, b) a,
typedef enum {ASPECT_RATIOS} ENUM_ASPECT_RATIOS_TYPES;

////------ Defines to be shared -------
#define STEP_CHANGE_VOLUME          10
#define STEP_CHANGE_BRIGHTNESS      10
#define NOTIF_SECONDS_DISP			2

////------ Menu commands -------
#define SHELL_CMD_VOLUME_GET        	"volume_get"
#define SHELL_CMD_VOLUME_SET        	"volume_set"
#define SHELL_CMD_BRIGHTNESS_GET    	"brightness_get"
#define SHELL_CMD_BRIGHTNESS_SET    	"brightness_set"
#define SHELL_CMD_POWERDOWN         	"shutdown_funkey"
#define SHELL_CMD_NOTIF					"notif_set"
#define SHELL_CMD_WRITE_QUICK_LOAD_CMD	"write_args_quick_load_file"

////------ Global variables -------
extern int volume_percentage;
extern int brightness_percentage;

extern const char *aspect_ratio_name[];
extern int aspect_ratio;
extern int aspect_ratio_factor_percent;
extern int aspect_ratio_factor_step;
extern int stop_menu_loop;

extern struct SAVE_STATE mSaveState[10];
extern s8 mSaveStateName[SAL_MAX_PATH];
//extern s8 mRomName[SAL_MAX_PATH];
extern s8 mSystemDir[SAL_MAX_PATH];

//####################################
//# Functions
//####################################
void MenuInit(const char *systemDir, struct MENU_OPTIONS *menuOptions, char *romName);
s32 MenuRun(s8 *romName);
//void LoadSram(const char *path, const char *romname, const char *ext, const char *srammem);
//void SaveSram(const char *path, const char *romname, const char *ext, const char *srammem);
//void DeleteSram(const char *path, const char *romname, const char *ext);
s32 SaveMenuOptions(const char *path, const char *filename, const char *ext,
			const char *optionsmem, s32 maxSize, s32 showMessage);
s32 LoadMenuOptions(const char *path, const char *filename, const char *ext,
			const char *optionsmem, s32 maxSize, s32 showMessage);
s32 DeleteMenuOptions(const char *path, const char *filename,
			const char *ext, s32 showMessage);
void DefaultMenuOptions(void);
void MenuPause(void);
void PrintTitle(const char *title);
void PrintTile();
void PrintBar(u32 givenY);
s32 MenuMessageBox(const char *message1, const char *message2,
			const char *message3, enum MENU_MESSAGE_BOX_MODE mode);
u32 IsPreviewingState();

bool LoadStateFile(s8 *filename);
bool SaveStateFile(s8 *filename);



void init_menu_SDL();
void deinit_menu_SDL();
void init_menu_zones();
void init_menu_system_values();
void run_menu_loop();


#endif /* _MENU_H_ */





