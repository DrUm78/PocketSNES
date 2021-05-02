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

///------ Definition of the different resume options
#define RESUME_OPTIONS \
    X(RESUME_YES, "RESUME GAME") \
    X(RESUME_NO, "NEW GAME") \
    X(NB_RESUME_OPTIONS, "")

////------ Enumeration of the different resume options ------
#undef X
#define X(a, b) a,
typedef enum {RESUME_OPTIONS} ENUM_RESUME_OPTIONS;

////------ Defines to be shared -------
#define STEP_CHANGE_VOLUME          10
#define STEP_CHANGE_BRIGHTNESS      10
#define NOTIF_SECONDS_DISP			2

////------ Menu commands -------
#define SHELL_CMD_VOLUME_GET                "volume get"
#define SHELL_CMD_VOLUME_SET                "volume set"
#define SHELL_CMD_BRIGHTNESS_GET            "brightness get"
#define SHELL_CMD_BRIGHTNESS_SET            "brightness set"
#define SHELL_CMD_NOTIF                     "notif_set"
#define SHELL_CMD_NOTIF_CLEAR               "notif_clear"
#define SHELL_CMD_WRITE_QUICK_LOAD_CMD      "write_args_quick_load_file"
#define SHELL_CMD_TURN_AMPLI_ON             "start_audio_amp 1"
#define SHELL_CMD_TURN_AMPLI_OFF            "start_audio_amp 0"
#define SHELL_CMD_CANCEL_SCHED_POWERDOWN    "cancel_sched_powerdown"
#define SHELL_CMD_INSTANT_PLAY              "instant_play"
#define SHELL_CMD_SHUTDOWN_FUNKEY           "shutdown_funkey"
#define SHELL_CMD_KEYMAP_DEFAULT            "keymap default"
#define SHELL_CMD_KEYMAP_RESUME             "keymap resume"

////------ Global variables -------
extern int volume_percentage;
extern int brightness_percentage;

extern int stop_menu_loop;

extern struct SAVE_STATE mSaveState[10];
extern s8 mSaveStateName[SAL_MAX_PATH];
//extern s8 mRomName[SAL_MAX_PATH];
extern s8 mSystemDir[SAL_MAX_PATH];
extern char *mRomName;
extern char *mRomPath;
extern char *cfg_file_rom;
extern char *quick_save_file;

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

int launch_resume_menu_loop();


#endif /* _MENU_H_ */





