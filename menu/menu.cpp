#include <errno.h>

#include "sal.h"
#include "menu.h"
#include "snapshot.h"
#include "snes9x.h"
#include "gfx.h"
#include "memmap.h"
#include "soundux.h"
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>



/// -------------- DEFINES --------------
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define ABS(x) (((x) < 0) ? (-x) : (x))

//#define MENU_DEBUG
#define MENU_ERROR

#ifdef MENU_DEBUG
#define MENU_DEBUG_PRINTF(...)   printf(__VA_ARGS__);
#else
#define MENU_DEBUG_PRINTF(...)
#endif //MENU_DEBUG

#ifdef MENU_ERROR
#define MENU_ERROR_PRINTF(...)   printf(__VA_ARGS__);
#else
#define MENU_ERROR_PRINTF(...)
#endif //MENU_ERROR

#define SCREEN_HORIZONTAL_SIZE      RES_HW_SCREEN_HORIZONTAL
#define SCREEN_VERTICAL_SIZE        RES_HW_SCREEN_VERTICAL

#define SCROLL_SPEED_PX             30
#define FPS_MENU                    50
#define ARROWS_PADDING              8

#define MENU_ZONE_WIDTH             SCREEN_HORIZONTAL_SIZE
#define MENU_ZONE_HEIGHT            SCREEN_VERTICAL_SIZE
#define MENU_BG_SQUARE_WIDTH        180
#define MENU_BG_SQUARE_HEIGHT       140

#define MENU_FONT_NAME_TITLE        "/usr/games/menu_resources/OpenSans-Bold.ttf"
#define MENU_FONT_SIZE_TITLE        22
#define MENU_FONT_NAME_INFO         "/usr/games/menu_resources/OpenSans-Bold.ttf"
#define MENU_FONT_SIZE_INFO         16
#define MENU_FONT_NAME_SMALL_INFO   "/usr/games/menu_resources/OpenSans-Regular.ttf"
#define MENU_FONT_SIZE_SMALL_INFO   13
#define MENU_PNG_BG_PATH            "/usr/games/menu_resources/zone_bg.png"
#define MENU_PNG_ARROW_TOP_PATH     "/usr/games/menu_resources/arrow_top.png"
#define MENU_PNG_ARROW_BOTTOM_PATH  "/usr/games/menu_resources/arrow_bottom.png"

#define GRAY_MAIN_R                 85
#define GRAY_MAIN_G                 85
#define GRAY_MAIN_B                 85
#define WHITE_MAIN_R                236
#define WHITE_MAIN_G                236
#define WHITE_MAIN_B                236

#define MAX_SAVE_SLOTS              9

#define MAXPATHLEN                  512

///------------------------------------------------------------
#define MAX_DISPLAY_CHARS			40

#define ROM_SELECTOR_SAVE_DEFAULT_DIR		0
#define ROM_SELECTOR_MAIN_MENU			1
#define ROM_SELECTOR_DEFAULT_FOCUS		2
#define ROM_SELECTOR_ROM_START			3

static u16 mMenuBackground[SAL_SCREEN_WIDTH * SAL_SCREEN_HEIGHT];

static s32 mMenutileXscroll=0;
static s32 mMenutileYscroll=0;
static s32 mTileCounter=0;
static s32 mQuickSavePresent=0;
static u32 mPreviewingState=0;

static s8 mMenuText[30][MAX_DISPLAY_CHARS];

static struct SAL_DIRECTORY_ENTRY *mRomList=NULL;
static s32 mRomCount;
static s8 mRomDir[SAL_MAX_PATH]={""};

struct SAVE_STATE mSaveState[10];  // holds the filenames for the savestate and "inuse" flags
s8 mSaveStateName[SAL_MAX_PATH]={""};       // holds the last filename to be scanned for save states
//s8 mRomName[SAL_MAX_PATH]={""};
s8 mSystemDir[SAL_MAX_PATH];

static struct MENU_OPTIONS *mMenuOptions=NULL;
static u16 mTempFb[SNES_WIDTH*SNES_HEIGHT_EXTENDED];
///------------------------------------------------------------


/// -------------- STATIC VARIABLES --------------
extern SDL_Surface * hw_screen;
extern SDL_Surface * virtual_hw_screen; // this one is not rotated
SDL_Surface * draw_screen;

static int backup_key_repeat_delay, backup_key_repeat_interval;
static SDL_Surface * backup_hw_screen = NULL;

static TTF_Font *menu_title_font = NULL;
static TTF_Font *menu_info_font = NULL;
static TTF_Font *menu_small_info_font = NULL;
static SDL_Surface *img_arrow_top = NULL;
static SDL_Surface *img_arrow_bottom = NULL;
static SDL_Surface ** menu_zone_surfaces = NULL;
static int * idx_menus = NULL;
static int nb_menu_zones = 0;
static int menuItem = 0;
int stop_menu_loop = 0;

static SDL_Color text_color = {GRAY_MAIN_R, GRAY_MAIN_G, GRAY_MAIN_B};
static int padding_y_from_center_menu_zone = 18;
static uint16_t width_progress_bar = 100;
static uint16_t height_progress_bar = 20;
static uint16_t x_volume_bar = 0;
static uint16_t y_volume_bar = 0;
static uint16_t x_brightness_bar = 0;
static uint16_t y_brightness_bar = 0;

int volume_percentage = 0;
int brightness_percentage = 0;

#undef X
#define X(a, b) b,
const char *aspect_ratio_name[] = {ASPECT_RATIOS};
int aspect_ratio = ASPECT_RATIOS_TYPE_STRETCHED;
int aspect_ratio_factor_percent = 50;
int aspect_ratio_factor_step = 10;

#undef X
#define X(a, b) b,
const char *resume_options_str[] = {RESUME_OPTIONS};

static int quick_load_slot_chosen = 0;
int savestate_slot = 0;
extern u32 mExit;

/// -------------- FUNCTIONS DECLARATION --------------
extern "C" void S9xSaveSRAM(int showWarning);
static void ScanSaveStates(s8 *romname);
static void SaveStateTemp();
static void DeleteStateTemp();

/// -------------- FUNCTIONS IMPLEMENTATION --------------
void init_menu_SDL(){
    MENU_DEBUG_PRINTF("Init Menu\n");

    /// ----- Loading the fonts -----
    menu_title_font = TTF_OpenFont(MENU_FONT_NAME_TITLE, MENU_FONT_SIZE_TITLE);
    if(!menu_title_font){
        MENU_ERROR_PRINTF("ERROR in init_menu_SDL: Could not open menu font %s, %s\n", MENU_FONT_NAME_TITLE, SDL_GetError());
    }
    menu_info_font = TTF_OpenFont(MENU_FONT_NAME_INFO, MENU_FONT_SIZE_INFO);
    if(!menu_info_font){
        MENU_ERROR_PRINTF("ERROR in init_menu_SDL: Could not open menu font %s, %s\n", MENU_FONT_NAME_INFO, SDL_GetError());
    }
    menu_small_info_font = TTF_OpenFont(MENU_FONT_NAME_SMALL_INFO, MENU_FONT_SIZE_SMALL_INFO);
    if(!menu_small_info_font){
        MENU_ERROR_PRINTF("ERROR in init_menu_SDL: Could not open menu font %s, %s\n", MENU_FONT_NAME_SMALL_INFO, SDL_GetError());
    }

    /// ----- Copy virtual_hw_screen at init ------
    backup_hw_screen = SDL_CreateRGBSurface(SDL_SWSURFACE,
        virtual_hw_screen->w, virtual_hw_screen->h, 16, 0, 0, 0, 0);
    if(backup_hw_screen == NULL){
        MENU_ERROR_PRINTF("ERROR in init_menu_SDL: Could not create backup_hw_screen: %s\n", SDL_GetError());
    }

    draw_screen = SDL_CreateRGBSurface(SDL_SWSURFACE,
        virtual_hw_screen->w, virtual_hw_screen->h, 16, 0, 0, 0, 0);
    if(draw_screen == NULL){
        MENU_ERROR_PRINTF("ERROR Could not create draw_screen: %s\n", SDL_GetError());
    }

    /// ------ Load arrows imgs -------
    img_arrow_top = IMG_Load(MENU_PNG_ARROW_TOP_PATH);
    if(!img_arrow_top) {
        MENU_ERROR_PRINTF("ERROR IMG_Load: %s\n", IMG_GetError());
    }
    img_arrow_bottom = IMG_Load(MENU_PNG_ARROW_BOTTOM_PATH);
    if(!img_arrow_bottom) {
        MENU_ERROR_PRINTF("ERROR IMG_Load: %s\n", IMG_GetError());
    }

    /// ------ Init menu zones ------
    init_menu_zones();
}

void deinit_menu_SDL(){
    MENU_DEBUG_PRINTF("End Menu \n");

    /// ------ Close font -------
    TTF_CloseFont(menu_title_font);
    TTF_CloseFont(menu_info_font);
    TTF_CloseFont(menu_small_info_font);

    /// ------ Free Surfaces -------
    for(int i=0; i < nb_menu_zones; i++){
        SDL_FreeSurface(menu_zone_surfaces[i]);
    }
    SDL_FreeSurface(backup_hw_screen);
    SDL_FreeSurface(draw_screen);

    SDL_FreeSurface(img_arrow_top);
    SDL_FreeSurface(img_arrow_bottom);

    /// ------ Free Menu memory and reset vars -----
    if(idx_menus){
        free(idx_menus);
    }
    idx_menus=NULL;
    nb_menu_zones = 0;
}


void draw_progress_bar(SDL_Surface * surface, uint16_t x, uint16_t y, uint16_t width,
                        uint16_t height, uint8_t percentage, uint16_t nb_bars){
    /// ------ Init Variables ------
    uint16_t line_width = 1; //px
    uint16_t padding_bars_ratio = 3;
    uint16_t nb_full_bars = 0;

    /// ------ Check values ------
    percentage = (percentage > 100)?100:percentage;
    x = (x > (surface->w-1))?(surface->w-1):x;
    y = (y > surface->h-1)?(surface->h-1):y;
    width = (width < line_width*2+1)?(line_width*2+1):width;
    width = (width > surface->w-x-1)?(surface->w-x-1):width;
    height = (height < line_width*2+1)?(line_width*2+1):height;
    height = (height > surface->h-y-1)?(surface->h-y-1):height;
    uint16_t nb_bars_max = ( width * padding_bars_ratio  /  (line_width*2+1) + 1 ) / (padding_bars_ratio+1);
    nb_bars = (nb_bars > nb_bars_max)?nb_bars_max:nb_bars;
    uint16_t bar_width = (width / nb_bars)*padding_bars_ratio/(padding_bars_ratio+1)+1;
    uint16_t bar_padding_x = bar_width/padding_bars_ratio;
    nb_full_bars = nb_bars*percentage/100;

    /// ------ draw full bars ------
    for (int i = 0; i < nb_full_bars; ++i)
    {
        /// ---- draw one bar ----
        //MENU_DEBUG_PRINTF("Drawing filled bar %d\n", i);
        SDL_Rect rect = {x+ i*(bar_width +bar_padding_x),
            y, bar_width, height};
        SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, GRAY_MAIN_R, GRAY_MAIN_G, GRAY_MAIN_B));
    }

    /// ------ draw full bars ------
    for (int i = 0; i < (nb_bars-nb_full_bars); ++i)
    {
        /// ---- draw one bar ----
        //MENU_DEBUG_PRINTF("Drawing empty bar %d\n", i);
        SDL_Rect rect = {x+ i*(bar_width +bar_padding_x) + nb_full_bars*(bar_width +bar_padding_x),
            y, bar_width, height};
        SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, GRAY_MAIN_R, GRAY_MAIN_G, GRAY_MAIN_B));

        SDL_Rect rect2 = {x+ i*(bar_width +bar_padding_x) + line_width + nb_full_bars*(bar_width +bar_padding_x),
            y + line_width, bar_width - line_width*2, height - line_width*2};
        SDL_FillRect(surface, &rect2, SDL_MapRGB(surface->format, WHITE_MAIN_R, WHITE_MAIN_R, WHITE_MAIN_R));
    }


}


void add_menu_zone(ENUM_MENU_TYPE menu_type){
    /// ------ Increase nb of menu zones -------
    nb_menu_zones++;

    /// ------ Realoc idx Menus array -------
    if(!idx_menus){
        idx_menus = (int*) malloc(nb_menu_zones*sizeof(int));
        menu_zone_surfaces = (SDL_Surface**) malloc(nb_menu_zones*sizeof(SDL_Surface*));
    }
    else{
        int *temp = (int*) realloc(idx_menus, nb_menu_zones*sizeof(int));
        idx_menus = temp;
        menu_zone_surfaces = (SDL_Surface**) realloc(menu_zone_surfaces, nb_menu_zones*sizeof(SDL_Surface*));
    }
    idx_menus[nb_menu_zones-1] = menu_type;

    /// ------ Reinit menu surface with height increased -------
    menu_zone_surfaces[nb_menu_zones-1] = IMG_Load(MENU_PNG_BG_PATH);
    if(!menu_zone_surfaces[nb_menu_zones-1]) {
        MENU_ERROR_PRINTF("ERROR IMG_Load: %s\n", IMG_GetError());
    }
    /// --------- Init Common Variables --------
    SDL_Surface *text_surface = NULL;
    SDL_Surface *surface = menu_zone_surfaces[nb_menu_zones-1];
    SDL_Rect text_pos;

    /// --------- Add new zone ---------
    switch(menu_type){
    case MENU_TYPE_VOLUME:
        MENU_DEBUG_PRINTF("Init MENU_TYPE_VOLUME\n");
        /// ------ Text ------
        text_surface = TTF_RenderText_Blended(menu_title_font, "VOLUME", text_color);
        text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
        text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone;
        SDL_BlitSurface(text_surface, NULL, surface, &text_pos);

        x_volume_bar = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - width_progress_bar)/2;
        y_volume_bar = surface->h - MENU_ZONE_HEIGHT/2 - height_progress_bar/2 + padding_y_from_center_menu_zone;
        draw_progress_bar(surface, x_volume_bar, y_volume_bar,
            width_progress_bar, height_progress_bar, 0, 100/STEP_CHANGE_VOLUME);
        break;
    case MENU_TYPE_BRIGHTNESS:
        MENU_DEBUG_PRINTF("Init MENU_TYPE_BRIGHTNESS\n");
        /// ------ Text ------
        text_surface = TTF_RenderText_Blended(menu_title_font, "BRIGHTNESS", text_color);
        text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
        text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone;
        SDL_BlitSurface(text_surface, NULL, surface, &text_pos);

        x_brightness_bar = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - width_progress_bar)/2;
        y_brightness_bar = surface->h - MENU_ZONE_HEIGHT/2 - height_progress_bar/2 + padding_y_from_center_menu_zone;
        draw_progress_bar(surface, x_brightness_bar, y_brightness_bar,
            width_progress_bar, height_progress_bar, 0, 100/STEP_CHANGE_BRIGHTNESS);
        break;
    case MENU_TYPE_SAVE:
        MENU_DEBUG_PRINTF("Init MENU_TYPE_SAVE\n");
        /// ------ Text ------
        text_surface = TTF_RenderText_Blended(menu_title_font, "SAVE", text_color);
        text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
        text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone*2;
        SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
        break;
    case MENU_TYPE_LOAD:
        MENU_DEBUG_PRINTF("Init MENU_TYPE_LOAD\n");
        /// ------ Text ------
        text_surface = TTF_RenderText_Blended(menu_title_font, "LOAD", text_color);
        text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
        text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone*2;
        SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
        break;
    case MENU_TYPE_ASPECT_RATIO:
        MENU_DEBUG_PRINTF("Init MENU_TYPE_ASPECT_RATIO\n");
        /// ------ Text ------
        text_surface = TTF_RenderText_Blended(menu_title_font, "ASPECT RATIO", text_color);
        text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
        text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone;
        SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
        break;
    case MENU_TYPE_EXIT:
        MENU_DEBUG_PRINTF("Init MENU_TYPE_EXIT\n");
        /// ------ Text ------
        text_surface = TTF_RenderText_Blended(menu_title_font, "EXIT GAME", text_color);
        text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
        text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
        SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
        break;
    case MENU_TYPE_POWERDOWN:
        MENU_DEBUG_PRINTF("Init MENU_TYPE_POWERDOWN\n");
        /// ------ Text ------
        text_surface = TTF_RenderText_Blended(menu_title_font, "POWERDOWN", text_color);
        text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
        text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
        SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
        break;
    default:
        MENU_DEBUG_PRINTF("Warning - In add_menu_zone, unknown MENU_TYPE: %d\n", menu_type);
        break;
    }

    /// ------ Free Surfaces -------
    SDL_FreeSurface(text_surface);
}

void init_menu_zones(){
    /// Init Volume Menu
    add_menu_zone(MENU_TYPE_VOLUME);
    /// Init Brightness Menu
    add_menu_zone(MENU_TYPE_BRIGHTNESS);
    /// Init Save Menu
    add_menu_zone(MENU_TYPE_SAVE);
    /// Init Load Menu
    add_menu_zone(MENU_TYPE_LOAD);
    /// Init Aspect Ratio Menu
    add_menu_zone(MENU_TYPE_ASPECT_RATIO);
    /// Init Exit Menu
    add_menu_zone(MENU_TYPE_EXIT);
    /// Init Powerdown Menu
    //add_menu_zone(MENU_TYPE_POWERDOWN);
}


void init_menu_system_values(){
    FILE *fp;
    char res[100];

    /// ------- Get system volume percentage --------
    fp = popen(SHELL_CMD_VOLUME_GET, "r");
    if (fp == NULL) {
        MENU_ERROR_PRINTF("Failed to run command %s\n", SHELL_CMD_VOLUME_GET );
        volume_percentage = 50; ///wrong value: setting default to 50
    }
    else{
        fgets(res, sizeof(res)-1, fp);

        /// Check if Volume is a number (at least the first char)
        if(res[0] < '0' || res[0] > '9'){
            MENU_ERROR_PRINTF("Wrong return value: %s for volume cmd: %s\n",res, SHELL_CMD_VOLUME_GET);
            volume_percentage = 50; ///wrong value: setting default to 50
        }
        else{
            volume_percentage = atoi(res);
            MENU_DEBUG_PRINTF("System volume = %d%%\n", volume_percentage);
        }
    }

    /// ------- Get system brightness percentage -------
    fp = popen(SHELL_CMD_BRIGHTNESS_GET, "r");
    if (fp == NULL) {
        MENU_ERROR_PRINTF("Failed to run command %s\n", SHELL_CMD_BRIGHTNESS_GET );
        brightness_percentage = 50; ///wrong value: setting default to 50
    }
    else{
        fgets(res, sizeof(res)-1, fp);

        /// Check if brightness is a number (at least the first char)
        if(res[0] < '0' || res[0] > '9'){
            MENU_ERROR_PRINTF("Wrong return value: %s for volume cmd: %s\n",res, SHELL_CMD_BRIGHTNESS_GET);
            brightness_percentage = 50; ///wrong value: setting default to 50
        }
        else{
            brightness_percentage = atoi(res);
            MENU_DEBUG_PRINTF("System brightness = %d%%\n", brightness_percentage);
        }
    }

    /// ------ Save prev key repeat params and set new Key repeat -------
    SDL_GetKeyRepeat(&backup_key_repeat_delay, &backup_key_repeat_interval);
    if(SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL)){
        MENU_ERROR_PRINTF("ERROR with SDL_EnableKeyRepeat: %s\n", SDL_GetError());
    }

    /// Get save slot from game
    savestate_slot = (savestate_slot%MAX_SAVE_SLOTS); // security
}

void menu_screen_refresh(int menuItem, int prevItem, int scroll, uint8_t menu_confirmation, uint8_t menu_action){
    /// --------- Vars ---------
    int print_arrows = (scroll==0)?1:0;

    /// --------- Clear HW screen ----------
    if(SDL_BlitSurface(backup_hw_screen, NULL, draw_screen, NULL)){
        MENU_ERROR_PRINTF("ERROR Could not Clear draw_screen: %s\n", SDL_GetError());
    }

    /// --------- Setup Blit Window ----------
    SDL_Rect menu_blit_window;
    menu_blit_window.x = 0;
    menu_blit_window.w = SCREEN_HORIZONTAL_SIZE;

    /// --------- Blit prev menu Zone going away ----------
    menu_blit_window.y = scroll;
    menu_blit_window.h = SCREEN_VERTICAL_SIZE;
    if(SDL_BlitSurface(menu_zone_surfaces[prevItem], &menu_blit_window, draw_screen, NULL)){
        MENU_ERROR_PRINTF("ERROR Could not Blit surface on virtual_hw_screen: %s\n", SDL_GetError());
    }

    /// --------- Blit new menu Zone going in (only during animations) ----------
    if(scroll>0){
        menu_blit_window.y = SCREEN_VERTICAL_SIZE-scroll;
        menu_blit_window.h = SCREEN_VERTICAL_SIZE;
        if(SDL_BlitSurface(menu_zone_surfaces[menuItem], NULL, draw_screen, &menu_blit_window)){
            MENU_ERROR_PRINTF("ERROR Could not Blit surface on draw_screen: %s\n", SDL_GetError());
        }
    }
    else if(scroll<0){
        menu_blit_window.y = SCREEN_VERTICAL_SIZE+scroll;
        menu_blit_window.h = SCREEN_VERTICAL_SIZE;
        if(SDL_BlitSurface(menu_zone_surfaces[menuItem], &menu_blit_window, draw_screen, NULL)){
            MENU_ERROR_PRINTF("ERROR Could not Blit surface on draw_screen: %s\n", SDL_GetError());
        }
    }
    /// --------- No Scroll ? Blitting menu-specific info
    else{
        SDL_Surface * text_surface = NULL;
        char text_tmp[40];
        SDL_Rect text_pos;
        char fname[MAXPATHLEN];
        uint16_t limit_filename_size = 20;
        memset(fname, 0, MAXPATHLEN);

        switch(idx_menus[menuItem]){
        case MENU_TYPE_VOLUME:
            draw_progress_bar(draw_screen, x_volume_bar, y_volume_bar,
                            width_progress_bar, height_progress_bar, volume_percentage, 100/STEP_CHANGE_VOLUME);
            break;

        case MENU_TYPE_BRIGHTNESS:
            draw_progress_bar(draw_screen, x_volume_bar, y_volume_bar,
                            width_progress_bar, height_progress_bar, brightness_percentage, 100/STEP_CHANGE_BRIGHTNESS);
            break;

        case MENU_TYPE_SAVE:
            /// ---- Write slot -----
            sprintf(text_tmp, "IN SLOT   < %d >", savestate_slot+1);
            text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
            text_pos.x = (draw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
            text_pos.y = draw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
            SDL_BlitSurface(text_surface, NULL, draw_screen, &text_pos);

            if(menu_action){
                sprintf(text_tmp, "Saving...");
                text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
            }
            else{
                if(menu_confirmation){
                    sprintf(text_tmp, "Are you sure ?");
                    text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
                }
                else{
                    /// ---- Write current Save state ----
                    if(mSaveState[savestate_slot].inUse)
                    {
                        printf("Found Save slot: %s\n", fname);
                        strcpy(fname, mSaveState[savestate_slot].filename);
                        if(strlen(fname) > limit_filename_size){fname[limit_filename_size]=0;} //limiting size
                        text_surface = TTF_RenderText_Blended(menu_small_info_font,fname, text_color);
                    }
                    else{
                        text_surface = TTF_RenderText_Blended(menu_info_font, "Free", text_color);
                    }
                }
            }
            text_pos.x = (draw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
            text_pos.y = draw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
            SDL_BlitSurface(text_surface, NULL, draw_screen, &text_pos);
            break;

        case MENU_TYPE_LOAD:
            /// ---- Write slot -----
		if(quick_load_slot_chosen){
		sprintf(text_tmp, "FROM AUTO SAVE");
            }
            else{
		sprintf(text_tmp, "FROM SLOT   < %d >", savestate_slot+1);
            }
	        text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
            text_pos.x = (draw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
            text_pos.y = draw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
            SDL_BlitSurface(text_surface, NULL, draw_screen, &text_pos);

            if(menu_action){
                sprintf(text_tmp, "Loading...");
                text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
            }
            else{
                if(menu_confirmation){
                    sprintf(text_tmp, "Are you sure ?");
                    text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
                }
                else {
			if(quick_load_slot_chosen){
				text_surface = TTF_RenderText_Blended(menu_info_font, " ", text_color);
			}
			else{
	                    /// ---- Get current Load state ----
	                    if(mSaveState[savestate_slot].inUse)
	                    {
	                        printf("Found Load slot: %s\n", fname);
	                        strcpy(fname, mSaveState[savestate_slot].filename);
	                        if(strlen(fname) > limit_filename_size){fname[limit_filename_size]=0;} //limiting size
	                        text_surface = TTF_RenderText_Blended(menu_small_info_font,fname, text_color);
	                    }
	                    else{
	                        text_surface = TTF_RenderText_Blended(menu_info_font, "Free", text_color);
	                    }
			}
                }
            }
            text_pos.x = (draw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
            text_pos.y = draw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
            SDL_BlitSurface(text_surface, NULL, draw_screen, &text_pos);
            break;

        case MENU_TYPE_ASPECT_RATIO:
            sprintf(text_tmp, "<   %s   >", aspect_ratio_name[aspect_ratio]);
            text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
            text_pos.x = (draw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
            text_pos.y = draw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + padding_y_from_center_menu_zone;
            SDL_BlitSurface(text_surface, NULL, draw_screen, &text_pos);
            break;

        case MENU_TYPE_EXIT:
        case MENU_TYPE_POWERDOWN:
            if(menu_confirmation){
                sprintf(text_tmp, "Are you sure ?");
                text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
                text_pos.x = (draw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
                text_pos.y = draw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
                SDL_BlitSurface(text_surface, NULL, draw_screen, &text_pos);
            }
            break;
        default:
            break;
        }

        /// ------ Free Surfaces -------
        if(text_surface)
             SDL_FreeSurface(text_surface);
    }

    /// --------- Print arrows --------
    if(print_arrows){
        /// Top arrow
        SDL_Rect pos_arrow_top;
        pos_arrow_top.x = (draw_screen->w - img_arrow_top->w)/2;
        pos_arrow_top.y = (draw_screen->h - MENU_BG_SQUARE_HEIGHT)/4 - img_arrow_top->h/2;
        SDL_BlitSurface(img_arrow_top, NULL, draw_screen, &pos_arrow_top);

        /// Bottom arrow
        SDL_Rect pos_arrow_bottom;
        pos_arrow_bottom.x = (draw_screen->w - img_arrow_bottom->w)/2;
        pos_arrow_bottom.y = draw_screen->h -
            (draw_screen->h - MENU_BG_SQUARE_HEIGHT)/4 - img_arrow_bottom->h/2;
        SDL_BlitSurface(img_arrow_bottom, NULL, draw_screen, &pos_arrow_bottom);
    }

    /// --------- Screen Rotate --------
    //SDL_Rotate_270(draw_screen, hw_screen);
    SDL_BlitSurface(draw_screen, NULL, hw_screen, NULL);

    /// --------- Flip Screen ----------
    SDL_Flip(hw_screen);
}


void run_menu_loop()
{
    MENU_DEBUG_PRINTF("Launch Menu\n");

    SDL_Event event;
    uint32_t prev_ms = SDL_GetTicks();
    uint32_t cur_ms = SDL_GetTicks();
    int scroll=0;
    int start_scroll=0;
    uint8_t screen_refresh = 1;
    char shell_cmd[100];
    FILE *fp;
    uint8_t menu_confirmation = 0;
    stop_menu_loop = 0;
    char fname[MAXPATHLEN];

    /// ------ Get init values -------
    init_menu_system_values();
    int prevItem=menuItem;

    /// ------ Copy currently displayed screen -------
    /*if(SDL_BlitSurface(virtual_hw_screen, NULL, backup_hw_screen, NULL)){
        MENU_ERROR_PRINTF("ERROR Could not copy virtual_hw_screen: %s\n", SDL_GetError());
    }*/
    uint16_t *dst_virtual = (uint16_t*) sal_VideoGetBuffer();
	memcpy(backup_hw_screen->pixels, dst_virtual,
			RES_HW_SCREEN_HORIZONTAL * RES_HW_SCREEN_VERTICAL * sizeof(u16));

    /// -------- Main loop ---------
    while (!stop_menu_loop)
    {
        /// -------- Handle Keyboard Events ---------
        if(!scroll){
            while (SDL_PollEvent(&event))
            switch(event.type)
            {
                case SDL_QUIT:
                    mExit = 1;
                    stop_menu_loop = 1;
		break;
	    case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_b:
                        if(menu_confirmation){
                            /// ------ Reset menu confirmation ------
                            menu_confirmation = 0;
                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        /*else{
                            stop_menu_loop = 1;
                        }*/
                        break;

                    case SDLK_q:
                    case SDLK_ESCAPE:
                        stop_menu_loop = 1;
                        break;

                    case SDLK_d:
                    case SDLK_DOWN:
                        MENU_DEBUG_PRINTF("DOWN\n");
                        /// ------ Start scrolling to new menu -------
                        menuItem++;
                        if (menuItem>=nb_menu_zones) menuItem=0;
                        scroll=1;

                        /// ------ Reset menu confirmation ------
                        menu_confirmation = 0;

                        /// ------ Refresh screen ------
                        screen_refresh = 1;
                        break;

                    case SDLK_u:
                    case SDLK_UP:
                        MENU_DEBUG_PRINTF("UP\n");
                        /// ------ Start scrolling to new menu -------
                        menuItem--;
                        if (menuItem<0) menuItem=nb_menu_zones-1;
                        scroll=-1;

                        /// ------ Reset menu confirmation ------
                        menu_confirmation = 0;

                        /// ------ Refresh screen ------
                        screen_refresh = 1;
                        break;

                    case SDLK_l:
                    case SDLK_LEFT:
                        //MENU_DEBUG_PRINTF("LEFT\n");
                        if(idx_menus[menuItem] == MENU_TYPE_VOLUME){
                            MENU_DEBUG_PRINTF("Volume DOWN\n");
                            /// ----- Compute new value -----
                            volume_percentage = (volume_percentage < STEP_CHANGE_VOLUME)?
                                                    0:(volume_percentage-STEP_CHANGE_VOLUME);

                            /// ----- Shell cmd ----
                            sprintf(shell_cmd, "%s %d", SHELL_CMD_VOLUME_SET, volume_percentage);
                            fp = popen(shell_cmd, "r");
                            if (fp == NULL) {
                                MENU_ERROR_PRINTF("Failed to run command %s\n", shell_cmd);
                            }

                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_BRIGHTNESS){
                            MENU_DEBUG_PRINTF("Brightness DOWN\n");
                            /// ----- Compute new value -----
                            brightness_percentage = (brightness_percentage < STEP_CHANGE_BRIGHTNESS)?
                                                    0:(brightness_percentage-STEP_CHANGE_BRIGHTNESS);

                            /// ----- Shell cmd ----
                            sprintf(shell_cmd, "%s %d", SHELL_CMD_BRIGHTNESS_SET, brightness_percentage);
                            fp = popen(shell_cmd, "r");
                            if (fp == NULL) {
                                MENU_ERROR_PRINTF("Failed to run command %s\n", shell_cmd);
                            }
                        /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_SAVE){
                            MENU_DEBUG_PRINTF("Save Slot DOWN\n");
                            savestate_slot = (!savestate_slot)?(MAX_SAVE_SLOTS-1):(savestate_slot-1);
                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_LOAD){
                            MENU_DEBUG_PRINTF("Load Slot DOWN\n");

                            /** Choose quick save file or standard saveslot for loading */
                            if(!quick_load_slot_chosen &&
			        savestate_slot == 0 &&
			        access(quick_save_file, F_OK ) != -1){
			        quick_load_slot_chosen = 1;
                            }
                            else if(quick_load_slot_chosen){
			        quick_load_slot_chosen = 0;
				savestate_slot = MAX_SAVE_SLOTS-1;
                            }
                            else{
			        savestate_slot = (!savestate_slot)?(MAX_SAVE_SLOTS-1):(savestate_slot-1);
                            }

                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_ASPECT_RATIO){
                            MENU_DEBUG_PRINTF("Aspect Ratio DOWN\n");
                            aspect_ratio = (!aspect_ratio)?(NB_ASPECT_RATIOS_TYPES-1):(aspect_ratio-1);
                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        break;

                    case SDLK_r:
                    case SDLK_RIGHT:
                        //MENU_DEBUG_PRINTF("RIGHT\n");
                        if(idx_menus[menuItem] == MENU_TYPE_VOLUME){
                            MENU_DEBUG_PRINTF("Volume UP\n");
                            /// ----- Compute new value -----
                            volume_percentage = (volume_percentage > 100 - STEP_CHANGE_VOLUME)?
                                                    100:(volume_percentage+STEP_CHANGE_VOLUME);

                            /// ----- Shell cmd ----
                            sprintf(shell_cmd, "%s %d", SHELL_CMD_VOLUME_SET, volume_percentage);
                            fp = popen(shell_cmd, "r");
                            if (fp == NULL) {
                                MENU_ERROR_PRINTF("Failed to run command %s\n", shell_cmd);
                            }
                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_BRIGHTNESS){
                            MENU_DEBUG_PRINTF("Brightness UP\n");
                            /// ----- Compute new value -----
                            brightness_percentage = (brightness_percentage > 100 - STEP_CHANGE_BRIGHTNESS)?
                                                    100:(brightness_percentage+STEP_CHANGE_BRIGHTNESS);

                            /// ----- Shell cmd ----
                            sprintf(shell_cmd, "%s %d", SHELL_CMD_BRIGHTNESS_SET, brightness_percentage);
                            fp = popen(shell_cmd, "r");
                            if (fp == NULL) {
                                MENU_ERROR_PRINTF("Failed to run command %s\n", shell_cmd);
                            }
                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_SAVE){
                            MENU_DEBUG_PRINTF("Save Slot UP\n");
                            savestate_slot = (savestate_slot+1)%MAX_SAVE_SLOTS;
                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_LOAD){
                            MENU_DEBUG_PRINTF("Load Slot UP\n");

                            /** Choose quick save file or standard saveslot for loading */
                            if(!quick_load_slot_chosen &&
			        savestate_slot == MAX_SAVE_SLOTS-1 &&
			        access(quick_save_file, F_OK ) != -1){
			        quick_load_slot_chosen = 1;
                            }
                            else if(quick_load_slot_chosen){
			        quick_load_slot_chosen = 0;
				savestate_slot = 0;
                            }
                            else{
			        savestate_slot = (savestate_slot+1)%MAX_SAVE_SLOTS;
                            }

                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_ASPECT_RATIO){
                            MENU_DEBUG_PRINTF("Aspect Ratio UP\n");
                            aspect_ratio = (aspect_ratio+1)%NB_ASPECT_RATIOS_TYPES;
                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        break;

                    case SDLK_a:
                    case SDLK_RETURN:
                        if(idx_menus[menuItem] == MENU_TYPE_SAVE){
                            if(menu_confirmation){
                                MENU_DEBUG_PRINTF("Saving in slot %d\n", savestate_slot);
                                /// ------ Refresh Screen -------
                                menu_screen_refresh(menuItem, prevItem, scroll, menu_confirmation, 1);

                                /// ------ Save game ------
                                if(SaveStateFile(mSaveState[savestate_slot].fullFilename)){
                                    mSaveState[savestate_slot].inUse=1;
                                }

                                /// ----- Hud Msg -----
                                sprintf(shell_cmd, "%s %d \"        SAVED IN SLOT %d\"",
                                    SHELL_CMD_NOTIF, NOTIF_SECONDS_DISP, savestate_slot+1);
                                fp = popen(shell_cmd, "r");
                                if (fp == NULL) {
                                    MENU_ERROR_PRINTF("Failed to run command %s\n", shell_cmd);
                                }

                                stop_menu_loop = 1;
                            }
                            else{
                                MENU_DEBUG_PRINTF("Save game - asking confirmation\n");
                                menu_confirmation = 1;
                                /// ------ Refresh screen ------
                                screen_refresh = 1;
                            }
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_LOAD){
                            if(menu_confirmation){
                                MENU_DEBUG_PRINTF("Loading in slot %d\n", savestate_slot);
                                /// ------ Refresh Screen -------
                                menu_screen_refresh(menuItem, prevItem, scroll, menu_confirmation, 1);

                                /// ------ Load game ------
                                if(quick_load_slot_chosen){
					LoadStateFile(quick_save_file);
					}
					else{
					LoadStateFile(mSaveState[savestate_slot].fullFilename);
					}

                                /// ----- Hud Msg -----
                                if(quick_load_slot_chosen){
	                                sprintf(shell_cmd, "%s %d \"     LOADED FROM AUTO SAVE\"",
	                                    SHELL_CMD_NOTIF, NOTIF_SECONDS_DISP);
				}
				else{
					sprintf(shell_cmd, "%s %d \"      LOADED FROM SLOT %d\"",
	                                    SHELL_CMD_NOTIF, NOTIF_SECONDS_DISP, savestate_slot+1);
				}
                                fp = popen(shell_cmd, "r");
                                if (fp == NULL) {
                                    MENU_ERROR_PRINTF("Failed to run command %s\n", shell_cmd);
                                }

                                stop_menu_loop = 1;
                            }
                            else{
                                MENU_DEBUG_PRINTF("Save game - asking confirmation\n");
                                menu_confirmation = 1;
                                /// ------ Refresh screen ------
                                screen_refresh = 1;
                            }
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_EXIT){
                            MENU_DEBUG_PRINTF("Exit game\n");
                            if(menu_confirmation){
                                MENU_DEBUG_PRINTF("Exit game - confirmed\n");
                                /// ----- The game is quick saved here ----
                                if(!SaveStateFile((s8 *)quick_save_file)){
                                    MENU_ERROR_PRINTF("Quick save failed");
                                    return;
                                }

                                /// ----- Exit game and back to launcher ----
                                mExit = 1;
                                stop_menu_loop = 1;
                            }
                            else{
                                MENU_DEBUG_PRINTF("Exit game - asking confirmation\n");
                                menu_confirmation = 1;
                                /// ------ Refresh screen ------
                                screen_refresh = 1;
                            }
                        }
                        else if(idx_menus[menuItem] == MENU_TYPE_POWERDOWN){
                            if(menu_confirmation){
                                MENU_DEBUG_PRINTF("Powerdown - confirmed\n");
                                /// ----- Shell cmd ----
                                execlp(SHELL_CMD_SHUTDOWN_FUNKEY, SHELL_CMD_SHUTDOWN_FUNKEY, NULL);
                                MENU_ERROR_PRINTF("Failed to run command %s\n", SHELL_CMD_SHUTDOWN_FUNKEY);
                                exit(0);
                            }
                            else{
                                MENU_DEBUG_PRINTF("Powerdown - asking confirmation\n");
                                menu_confirmation = 1;
                                /// ------ Refresh screen ------
                                screen_refresh = 1;
                            }
                        }
                        break;

                    default:
                        //MENU_DEBUG_PRINTF("Keydown: %d\n", event.key.keysym.sym);
                        break;
                }
                break;
            }
        }

        /// --------- Handle Scroll effect ---------
        if ((scroll>0) || (start_scroll>0)){
            scroll+=MIN(SCROLL_SPEED_PX, MENU_ZONE_HEIGHT-scroll);
            start_scroll = 0;
            screen_refresh = 1;
        }
        else if ((scroll<0) || (start_scroll<0)){
            scroll-=MIN(SCROLL_SPEED_PX, MENU_ZONE_HEIGHT+scroll);
            start_scroll = 0;
            screen_refresh = 1;
        }
        if (scroll>=MENU_ZONE_HEIGHT || scroll<=-MENU_ZONE_HEIGHT) {
            prevItem=menuItem;
            scroll=0;
            screen_refresh = 1;
        }

        /// --------- Handle FPS ---------
        cur_ms = SDL_GetTicks();
        if(cur_ms-prev_ms < 1000/FPS_MENU){
            SDL_Delay(1000/FPS_MENU - (cur_ms-prev_ms));
        }
        prev_ms = SDL_GetTicks();


        /// --------- Refresh screen
        if(screen_refresh){
            menu_screen_refresh(menuItem, prevItem, scroll, menu_confirmation, 0);
        }

        /// --------- reset screen refresh ---------
        screen_refresh = 0;
    }

    /// ------ Reset prev key repeat params -------
    if(SDL_EnableKeyRepeat(backup_key_repeat_delay, backup_key_repeat_interval)){
        MENU_ERROR_PRINTF("ERROR with SDL_EnableKeyRepeat: %s\n", SDL_GetError());
    }
}



/****************************/
/*    Quick Resume Menu     */
/****************************/
int launch_resume_menu_loop()
{
    MENU_DEBUG_PRINTF("Init resume menu\n");

    /* Decare vars */
    SDL_Surface *text_surface = NULL;
    char text_tmp[40];
    SDL_Rect text_pos;
    SDL_Event event;
    uint32_t prev_ms = SDL_GetTicks();
    uint32_t cur_ms = SDL_GetTicks();
    stop_menu_loop = 0;
    uint8_t screen_refresh = 1;
    uint8_t menu_confirmation = 0;
    int option_idx=RESUME_YES;

    /* Save prev key repeat params and set new Key repeat */
    SDL_GetKeyRepeat(&backup_key_repeat_delay, &backup_key_repeat_interval);
    if(SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL)){
        MENU_ERROR_PRINTF("ERROR with SDL_EnableKeyRepeat: %s\n", SDL_GetError());
    }

    /* Load BG */
    SDL_Surface *img_square_bg = IMG_Load(MENU_PNG_BG_PATH);
    if(!img_square_bg) {
        MENU_ERROR_PRINTF("ERROR IMG_Load: %s\n", IMG_GetError());
    }
    SDL_Surface *bg_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, hw_screen->w, hw_screen->h, 16, 0, 0, 0, 0);
    SDL_BlitSurface(img_square_bg, NULL, bg_surface, NULL);
    SDL_FreeSurface(img_square_bg);


    /*  Print top arrow */
    SDL_Rect pos_arrow_top;
    pos_arrow_top.x = (bg_surface->w - img_arrow_top->w)/2;
    pos_arrow_top.y = (bg_surface->h - MENU_BG_SQUARE_HEIGHT)/4 - img_arrow_top->h/2;
    SDL_BlitSurface(img_arrow_top, NULL, bg_surface, &pos_arrow_top);

    /*  Print bottom arrow */
    SDL_Rect pos_arrow_bottom;
    pos_arrow_bottom.x = (bg_surface->w - img_arrow_bottom->w)/2;
    pos_arrow_bottom.y = bg_surface->h -
            (bg_surface->h - MENU_BG_SQUARE_HEIGHT)/4 - img_arrow_bottom->h/2;
    SDL_BlitSurface(img_arrow_bottom, NULL, bg_surface, &pos_arrow_bottom);

    if (text_surface)
        SDL_FreeSurface(text_surface);

    /* Main loop */
    while (!stop_menu_loop)
    {
        /* Handle keyboard events */
        while (SDL_PollEvent(&event))
        switch(event.type)
        {
            case SDL_QUIT:
                mExit = 1;
                stop_menu_loop = 1;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_b:
                        if(menu_confirmation){
                            /// ------ Reset menu confirmation ------
                            menu_confirmation = 0;

                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        /*else{
                            stop_menu_loop = 1;
                        }*/
                        break;

                    case SDLK_q:
                    case SDLK_ESCAPE:
                        /*stop_menu_loop = 1;*/
                        break;

                    case SDLK_u:
                    case SDLK_UP:
                        MENU_DEBUG_PRINTF("Option UP\n");
                        option_idx = (!option_idx)?(NB_RESUME_OPTIONS-1):(option_idx-1);

                        /// ------ Reset menu confirmation ------
                        menu_confirmation = 0;

                        /// ------ Refresh screen ------
                        screen_refresh = 1;
                        break;

                    case SDLK_d:
                    case SDLK_DOWN:
                        MENU_DEBUG_PRINTF("Option DWON\n");
                        option_idx = (option_idx+1)%NB_RESUME_OPTIONS;

                        /// ------ Reset menu confirmation ------
                        menu_confirmation = 0;

                        /// ------ Refresh screen ------
                        screen_refresh = 1;
                        break;

                    case SDLK_a:
                    case SDLK_RETURN:
                        MENU_DEBUG_PRINTF("Pressed A\n");
                        if(menu_confirmation){
                            MENU_DEBUG_PRINTF("Confirmed\n");

                            /// ----- exit menu  ----
                            stop_menu_loop = 1;
                        }
                        else{
                            MENU_DEBUG_PRINTF("Asking confirmation\n");
                            menu_confirmation = 1;

                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        break;

                    default:
                        //MENU_DEBUG_PRINTF("Keydown: %d\n", event.key.keysym.sym);
                        break;
            }
            break;
        }

        /* Handle FPS */
        cur_ms = SDL_GetTicks();
        if(cur_ms-prev_ms < 1000/FPS_MENU){
            SDL_Delay(1000/FPS_MENU - (cur_ms-prev_ms));
        }
        prev_ms = SDL_GetTicks();

        /* Refresh screen */
        if(screen_refresh){
            /* Clear and draw BG */
            SDL_FillRect(hw_screen, NULL, 0);
            if(SDL_BlitSurface(bg_surface, NULL, hw_screen, NULL)){
                MENU_ERROR_PRINTF("ERROR Could not draw background: %s\n", SDL_GetError());
            }

            /* Draw resume or reset option */
            text_surface = TTF_RenderText_Blended(menu_title_font, resume_options_str[option_idx], text_color);
            text_pos.x = (hw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
            text_pos.y = hw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
            SDL_BlitSurface(text_surface, NULL, hw_screen, &text_pos);

            /* Draw confirmation */
            if(menu_confirmation){
                sprintf(text_tmp, "Are you sure ?");
                text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
                text_pos.x = (hw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
                text_pos.y = hw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
                SDL_BlitSurface(text_surface, NULL, hw_screen, &text_pos);
            }

            /* Flip Screen */
            SDL_Flip(hw_screen);
        }

        /* reset screen refresh */
        screen_refresh = 0;
    }

    /* Free SDL Surfaces */
    if(bg_surface)
        SDL_FreeSurface(bg_surface);
    if(text_surface)
        SDL_FreeSurface(text_surface);

    /* Reset prev key repeat params */
    if(SDL_EnableKeyRepeat(backup_key_repeat_delay, backup_key_repeat_interval)){
        MENU_ERROR_PRINTF("ERROR with SDL_EnableKeyRepeat: %s\n", SDL_GetError());
    }

    return option_idx;
}

void DefaultMenuOptions(void)
{
	mMenuOptions->frameSkip=0;   //auto
	mMenuOptions->soundEnabled = 1;
	mMenuOptions->volume=25;
	mMenuOptions->cpuSpeed=336;
	mMenuOptions->country=0;
	mMenuOptions->showFps=0;
	mMenuOptions->soundRate=44100;
	mMenuOptions->stereo=1;
	mMenuOptions->fullScreen=0;
	mMenuOptions->autoSaveSram=1;
	mMenuOptions->soundSync=1;
}

s32 LoadMenuOptions(const char *path, const char *filename, const char *ext,
			const char *optionsmem, s32 maxSize, s32 showMessage)
{
	s8 fullFilename[SAL_MAX_PATH];
	s8 _filename[SAL_MAX_PATH];
	s8 _ext[SAL_MAX_PATH];
	s8 _path[SAL_MAX_PATH];
	s32 size=0;

	sal_DirectorySplitFilename(filename, _path, _filename, _ext);
	sprintf(fullFilename,"%s%s%s.%s",path,SAL_DIR_SEP,_filename,ext);
	return sal_FileLoad(fullFilename,(u8*)optionsmem,maxSize,(u32*)&size);
}

s32 SaveMenuOptions(const char *path, const char *filename, const char *ext,
			const char *optionsmem, s32 maxSize, s32 showMessage)
{
	s8 fullFilename[SAL_MAX_PATH];
	s8 _filename[SAL_MAX_PATH];
	s8 _ext[SAL_MAX_PATH];
	s8 _path[SAL_MAX_PATH];

	if (showMessage)
	{
		PrintTitle("");
		sal_VideoPrint(8,120,"Saving...",SAL_RGB(31,31,31));
		sal_VideoFlip(1);
	}

	sal_DirectorySplitFilename(filename, _path, _filename, _ext);
	sprintf(fullFilename,"%s%s%s.%s",path,SAL_DIR_SEP,_filename,ext);
	return sal_FileSave(fullFilename,(u8*)optionsmem,maxSize);
}

s32 DeleteMenuOptions(const char *path, const char *filename,
			const char *ext, s32 showMessage)
{
	s8 fullFilename[SAL_MAX_PATH];
	s8 _filename[SAL_MAX_PATH];
	s8 _ext[SAL_MAX_PATH];
	s8 _path[SAL_MAX_PATH];

	if (showMessage)
	{
		PrintTitle("");
		sal_VideoPrint(8,120,"Deleting...",SAL_RGB(31,31,31));
		sal_VideoFlip(1);
	}

	sal_DirectorySplitFilename(filename, _path, _filename, _ext);
	sprintf(fullFilename,"%s%s%s.%s",path,SAL_DIR_SEP,_filename,ext);
	sal_FileDelete(fullFilename);
	return SAL_OK;
}

s32 LoadLastSelectedRomPos() // Try to get the last selected rom position from a config file
{
	char lastselfile[SAL_MAX_PATH];
	s32 savedval = ROM_SELECTOR_DEFAULT_FOCUS;
	strcpy(lastselfile, sal_DirectoryGetHome());
	sal_DirectoryCombine(lastselfile, "lastselected.opt");
	FILE * pFile;
	pFile = fopen (lastselfile,"r+");
	if (pFile != NULL) {
		fscanf (pFile, "%i", &savedval);
		fclose (pFile);
	}
	return savedval;
}

void SaveLastSelectedRomPos(s32 pospointer) // Save the last selected rom position in a config file
{
	char lastselfile[SAL_MAX_PATH];
	strcpy(lastselfile, sal_DirectoryGetHome());
	sal_DirectoryCombine(lastselfile, "lastselected.opt");
	FILE * pFile;
	pFile = fopen (lastselfile,"w+");
	fprintf (pFile, "%i", pospointer);
	fclose (pFile);
}

void DelLastSelectedRomPos() // Remove the last selected rom position config file
{
	char lastselfile[SAL_MAX_PATH];
	strcpy(lastselfile, sal_DirectoryGetHome());
	sal_DirectoryCombine(lastselfile, "lastselected.opt");
	remove (lastselfile);
}

void MenuPause()
{
	sal_InputWaitForPress();
	sal_InputWaitForRelease();
}

s32 MenuMessageBox(const char *message1, const char *message2,
			const char *message3, enum MENU_MESSAGE_BOX_MODE mode)
{

  /// Return here for Funkey
	return 0;

  s32 select=0;
  s32 subaction=-1;
  u32 keys=0;

  sal_InputIgnore();
  while(subaction==-1)
  {
     keys=sal_InputPollRepeat();
     if (keys & SAL_INPUT_UP)
     {
       select=SAL_OK; // Up
     }
     if (keys & SAL_INPUT_DOWN)
     {
       select=SAL_ERROR; // Down
     }
     if ((keys&INP_BUTTON_MENU_SELECT) || (keys&INP_BUTTON_MENU_CANCEL))
     {
        subaction=select;
     }
     PrintTitle("Message Box");
     sal_VideoPrint(8,50,message1,SAL_RGB(31,31,31));
     sal_VideoPrint(8,60,message2,SAL_RGB(31,31,31));
     sal_VideoPrint(8,70,message3,SAL_RGB(31,31,31));
     switch(mode)
     {
        case MENU_MESSAGE_BOX_MODE_YESNO: // yes no input
	       if(select==SAL_OK)
	       {
			  PrintBar(120-4);
	          sal_VideoPrint(8,120,"YES",SAL_RGB(31,31,31));
	          sal_VideoPrint(8,140,"NO",SAL_RGB(31,31,31));
	       }
	       else
	       {
			  PrintBar(140-4);
	          sal_VideoPrint(8,120,"YES",SAL_RGB(31,31,31));
	          sal_VideoPrint(8,140,"NO",SAL_RGB(31,31,31));
	       }
	       break;
	case MENU_MESSAGE_BOX_MODE_PAUSE:
			PrintBar(120-4);
			sal_VideoPrint(8,120,"Press button to continue",SAL_RGB(31,31,31));
			break;
	case MENU_MESSAGE_BOX_MODE_MSG:
			subaction=SAL_OK;
			break;
     }
     sal_VideoFlip(1);
  }
  sal_InputIgnore();
  return(subaction);
}

void PrintTitle(const char *title)
{
	sal_ImageDraw(mMenuBackground,SAL_SCREEN_WIDTH, SAL_SCREEN_HEIGHT,0,0);
}

void PrintBar(u32 givenY)
{
	//sal_ImageDraw(mHighLightBar,HIGHLIGHT_BAR_WIDTH, HIGHLIGHT_BAR_HEIGHT,0,givenY);
	sal_HighlightBar( 262, HIGHLIGHT_BAR_HEIGHT, 0, givenY);
}

void freeRomLists()
{
	//free rom list buffers
	if(mRomList != NULL) free(mRomList);
	mRomList = NULL;
}

void DefaultRomListItems()
{
	s32 i;

	strcpy(mRomList[ROM_SELECTOR_SAVE_DEFAULT_DIR].displayName,"Save default directory");
	strcpy(mRomList[ROM_SELECTOR_MAIN_MENU].displayName,"Main menu");
	mRomList[ROM_SELECTOR_DEFAULT_FOCUS].displayName[0]=0;
}

static
void SwapDirectoryEntry(struct SAL_DIRECTORY_ENTRY *salFrom, struct SAL_DIRECTORY_ENTRY *salTo)
{
	struct SAL_DIRECTORY_ENTRY temp;

	//Copy salFrom to temp entry
	strcpy(temp.displayName, salFrom->displayName);
	strcpy(temp.filename, salFrom->filename);
	temp.type = salFrom->type;

	//Copy salTo to salFrom
	strcpy(salFrom->displayName, salTo->displayName);
	strcpy(salFrom->filename,salTo->filename);
	salFrom->type=salTo->type;

	//Copy temp entry to salTo
	strcpy(salTo->displayName, temp.displayName);
	strcpy(salTo->filename, temp.filename);
	salTo->type=temp.type;

}

int FileScan()
{
	s32 itemCount=0, fileCount=0, dirCount=0;
	s32 x,a,b,startIndex=ROM_SELECTOR_DEFAULT_FOCUS+1;
	s8 text[50];
	s8 filename[SAL_MAX_PATH];
	s8 path[SAL_MAX_PATH];
	s8 ext[SAL_MAX_PATH];
	struct SAL_DIR d;

	freeRomLists();

#if 0
	PrintTitle("File Scan");
	sal_VideoPrint(8,120,"Scanning Directory...",SAL_RGB(31,31,31));
	sal_VideoFlip(1);
#endif
	if(sal_DirectoryGetItemCount(mRomDir,&itemCount)==SAL_ERROR)
	{
		return SAL_ERROR;
	}

	mRomCount=ROM_SELECTOR_ROM_START+itemCount;

	mRomList=(SAL_DIRECTORY_ENTRY*)malloc(sizeof(struct SAL_DIRECTORY_ENTRY)*mRomCount);

	//was there enough memory?
	if(mRomList == NULL)
	{
		MenuMessageBox("Could not allocate memory","Too many files","",MENU_MESSAGE_BOX_MODE_PAUSE);
		//not enough memory - try the minimum
		mRomList=(SAL_DIRECTORY_ENTRY*)malloc(sizeof(struct SAL_DIRECTORY_ENTRY)*ROM_SELECTOR_ROM_START);
		mRomCount=ROM_SELECTOR_ROM_START;
		if (mRomList == NULL)
		{
			//still no joy
			MenuMessageBox("Dude, I'm really broken now","Restart system","never do this again",MENU_MESSAGE_BOX_MODE_PAUSE);
			mRomCount = -1;
			return SAL_ERROR;
		}
	}

	//Add default items
	DefaultRomListItems();

	if (itemCount>0)
	{
		if (sal_DirectoryOpen(mRomDir, &d)==SAL_OK)
		{
			//Dir opened, now stream out details
			x=0;
			while(sal_DirectoryRead(&d, &mRomList[x+startIndex])==SAL_OK)
			{
				//Dir entry read
#if 0
				PrintTitle("File Scan");
				sprintf(text,"Fetched item %d of %d",x, itemCount-1);
				sal_VideoPrint(8,120,text,SAL_RGB(31,31,31));
				PrintBar(228-4);
				sal_VideoPrint(0,228,mRomDir,SAL_RGB(0,0,0));
				sal_VideoFlip(1);
#endif
				if (mRomList[x+startIndex].type == SAL_FILE_TYPE_FILE)
				{
					sal_DirectorySplitFilename(mRomList[x+startIndex].filename,path,filename,ext);
					if(
						sal_StringCompare(ext,"zip") == 0 ||
						sal_StringCompare(ext,"smc") == 0 ||
						sal_StringCompare(ext,"sfc") == 0 ||
						sal_StringCompare(ext,"fig") == 0 /* Super WildCard dump */ ||
						sal_StringCompare(ext,"swc") == 0 /* Super WildCard dump */)
					{
						fileCount++;
						x++;
					}
				}
				else
				{
					dirCount++;
					x++;
				}

			}
			mRomCount=ROM_SELECTOR_ROM_START+dirCount+fileCount;
			sal_DirectoryClose(&d);
		}
		else
		{
			return SAL_ERROR;
		}

#if 0
		PrintTitle("File Scan");
		sal_VideoPrint(8,120,"Sorting items...",SAL_RGB(31,31,31));
		sal_VideoFlip(1);
#endif
		int lowIndex=0;
		//Put all directory entries at the top
		for(a=startIndex;a<startIndex+dirCount;a++)
		{
			if (mRomList[a].type == SAL_FILE_TYPE_FILE)
			{
				for(b=a+1;b<mRomCount;b++)
				{
					if (mRomList[b].type == SAL_FILE_TYPE_DIRECTORY)
					{
						SwapDirectoryEntry(&mRomList[a],&mRomList[b]);
						break;
					}
				}
			}
		}

		//Now sort directory entries
		for(a=startIndex;a<startIndex+dirCount;a++)
		{
			lowIndex=a;
			for(b=a+1;b<startIndex+dirCount;b++)
			{
				if (sal_StringCompare(mRomList[b].displayName, mRomList[lowIndex].displayName) < 0)
				{
					//this index is lower
					lowIndex=b;
				}
			}
			//lowIndex should index next lowest value
			SwapDirectoryEntry(&mRomList[lowIndex],&mRomList[a]);
		}

		//Now sort file entries
		for(a=startIndex+dirCount;a<mRomCount;a++)
		{
			lowIndex=a;
			for(b=a+1;b<mRomCount;b++)
			{
				if (sal_StringCompare(mRomList[b].displayName, mRomList[lowIndex].displayName) < 0)
				{
					//this index is lower
					lowIndex=b;
				}
			}
			//lowIndex should index next lowest value
			SwapDirectoryEntry(&mRomList[lowIndex],&mRomList[a]);
		}

	}

	return SAL_OK;
}

s32 UpdateRomCache()
{
	s8 filename[SAL_MAX_PATH];
	PrintTitle("CRC Lookup");
	sal_VideoPrint(8,120,"Saving cache to disk...",SAL_RGB(31,31,31));
	sal_VideoFlip(1);

	strcpy(filename,mRomDir);
	sal_DirectoryCombine(filename,"romcache.dat");
	sal_FileSave(filename, (u8*)&mRomList[0], sizeof(struct SAL_DIRECTORY_ENTRY)*(mRomCount));

	return SAL_OK;
}

s32 FileSelect()
{
	s8 text[SAL_MAX_PATH];
	s8 previewPath[SAL_MAX_PATH];
	s8 previousRom[SAL_MAX_PATH];
	u16 romPreview[262 * 186];
	bool8 havePreview = FALSE;
	s32 action=0;
	s32 smooth=0;
	u16 color=0;
	s32 i=0;
	s32 focus=ROM_SELECTOR_DEFAULT_FOCUS;
	s32 menuExit=0;
	s32 scanstart=0,scanend=0;
	u32 keys=0;
	s32 size=0, check=SAL_OK;

	previousRom[0] = '\0';

	if (FileScan() != SAL_OK)
	{
		strcpy(mRomDir, sal_DirectoryGetUser());
		if (FileScan() != SAL_OK)
		{
			MenuMessageBox("Home directory inaccessible","","",MENU_MESSAGE_BOX_MODE_PAUSE);
			mRomCount=ROM_SELECTOR_DEFAULT_FOCUS;
			menuExit = 1;
			return 0;
		}
	}

	focus = LoadLastSelectedRomPos(); //try to load a saved position in the romlist

	smooth=focus<<8;
	sal_InputIgnore();
	while (menuExit==0)
	{
		keys=sal_InputPollRepeat();

		if (keys & INP_BUTTON_MENU_SELECT)
		{
			switch(focus)
			{
				case ROM_SELECTOR_SAVE_DEFAULT_DIR: //Save default directory
					DelLastSelectedRomPos(); //delete any previously saved position in the romlist
					SaveMenuOptions(mSystemDir, DEFAULT_ROM_DIR_FILENAME, DEFAULT_ROM_DIR_EXT, mRomDir, strlen(mRomDir), 1);
					break;

				case ROM_SELECTOR_MAIN_MENU: //Return to menu
					action=0;
					menuExit=1;
					break;

				case ROM_SELECTOR_DEFAULT_FOCUS: //blank space - do nothing
					break;

				default:
					// normal file or dir selected
					if (mRomList[focus].type == SAL_FILE_TYPE_DIRECTORY)
					{
						//Check for special directory names "." and ".."
						if (sal_StringCompare(mRomList[focus].filename,".") == 0)
						{
							//goto root directory

						}
						else if (sal_StringCompare(mRomList[focus].filename,"..") == 0)
						{
							// up a directory
							//Remove a directory from RomPath and rescan
							//Code below will never let you go further up than \SD Card\ on the Gizmondo
							//This is by design.
							sal_DirectoryGetParent(mRomDir);
							FileScan();
							focus=ROM_SELECTOR_DEFAULT_FOCUS; // default menu to non menu item
														// just to stop directory scan being started
							smooth=focus<<8;
							sal_InputIgnore();
							break;
						}
						else
						{
							//go to sub directory
							sal_DirectoryCombine(mRomDir,mRomList[focus].filename);
							FileScan();
							focus=ROM_SELECTOR_DEFAULT_FOCUS; // default menu to non menu item
														// just to stop directory scan being started
							smooth=focus<<8;
						}
					}
					else
					{
						// user has selected a rom, so load it
						SaveLastSelectedRomPos(focus); // save the current position in the romlist
						strcpy(mRomName, mRomDir);
						sal_DirectoryCombine(mRomName,mRomList[focus].filename);
						mQuickSavePresent=0;  // reset any quick saves
						action=1;
						menuExit=1;
					}
					sal_InputIgnore();
					break;
			}
		}
		else if (keys & INP_BUTTON_MENU_CANCEL) {
			sal_InputWaitForRelease();

			action=0;
			menuExit=1;
		}
		else if ((keys & (SAL_INPUT_UP | SAL_INPUT_DOWN))
		      && (keys & (SAL_INPUT_UP | SAL_INPUT_DOWN)) != (SAL_INPUT_UP | SAL_INPUT_DOWN))
		{
			if (keys & SAL_INPUT_UP)
				focus--; // Up
			else if (keys & SAL_INPUT_DOWN)
				focus++; // Down
		}
		else if ((keys & (SAL_INPUT_LEFT | SAL_INPUT_RIGHT))
		      && (keys & (SAL_INPUT_LEFT | SAL_INPUT_RIGHT)) != (SAL_INPUT_LEFT | SAL_INPUT_RIGHT))
		{
			if (keys & SAL_INPUT_LEFT)
				focus-=12;
			else if (keys & SAL_INPUT_RIGHT)
				focus+=12;

			if (focus>mRomCount-1)
				focus=mRomCount-1;
			else if (focus<0)
				focus=0;

			smooth=(focus<<8)-1;
		}

		if (focus>mRomCount-1)
		{
			focus=0;
			smooth=(focus<<8)-1;
		}
		else if (focus<0)
		{
			focus=mRomCount-1;
			smooth=(focus<<8)-1;
		}

		// Draw screen:
		PrintTitle("ROM selection");

		if (strcmp(mRomList[focus].displayName, previousRom) != 0) {
			char dummy[SAL_MAX_PATH], fileNameNoExt[SAL_MAX_PATH];
			sal_DirectorySplitFilename(mRomList[focus].filename, dummy, fileNameNoExt, dummy);
			sprintf(previewPath, "%s/previews/%s.%s", sal_DirectoryGetHome(), fileNameNoExt, "png");
			strcpy(previousRom, mRomList[focus].displayName);
			havePreview = sal_ImageLoad(previewPath, &romPreview, 262, 186) != SAL_ERROR;
			if (havePreview) {
				sal_VideoBitmapDim(romPreview, 262 * 186);
			}
		}

		if (havePreview) {
			sal_ImageDraw(romPreview, 262, 186, 0, 16);
		}

		smooth=smooth*7+(focus<<8); smooth>>=3;

		scanstart=focus-15;
		if (scanstart<0) scanstart=0;
		scanend = focus+15;
		if (scanend>mRomCount) scanend=mRomCount;

		for (i=scanstart;i<scanend;i++)
		{
			s32 x=0,y=0;

			y=(i<<4)-(smooth>>4);
			x=0;
			y+=112 - 28;
			if (y<=48 - 28 || y>=232 - 36) continue;

			if (i==focus)
			{
				color=SAL_RGB(31,31,31);
				PrintBar(y-4);
			}
			else
			{
				color=SAL_RGB(31,31,31);
			}


			// Draw Directory icon if current entry is a directory
			if(mRomList[i].type == SAL_FILE_TYPE_DIRECTORY)
			{
				sprintf(text,"<%s>",mRomList[i].displayName);
				sal_VideoPrint(x,y,text,color);
			}
			else
			{
				sal_VideoPrint(x,y,mRomList[i].displayName,color);
			}


		}

		sal_VideoPrint(0,4,mRomDir,SAL_RGB(31,8,8));

		sal_VideoFlip(1);
		usleep(10000);
	}
	sal_InputIgnore();

	freeRomLists();

	return action;
}

static void ScanSaveStates(s8 *romname)
{
	s32 i=0;
	s8 savename[SAL_MAX_PATH];
	s8 filename[SAL_MAX_PATH];
	s8 ext[SAL_MAX_PATH];
	s8 path[SAL_MAX_PATH];

	if(!strcmp(romname,mSaveStateName))
	{
        	printf("In ScanSaveStates, is current save state rom so exit, %s\n", romname);
		return; // is current save state rom so exit
	}

	sal_DirectorySplitFilename(romname,path,filename,ext);

	sprintf(savename,"%s.%s",filename,SAVESTATE_EXT);

	for(i=0;i<10;i++)
	{
		/*
		need to build a save state filename
		all saves are held in current working directory (lynxSaveStateDir)
		save filename has following format
		shortname(minus file ext) + SV + saveno ( 0 to 9 )
		*/
		sprintf(mSaveState[i].filename,"%s%d",savename,i);
		sprintf(mSaveState[i].fullFilename,"%s%s%s",mSystemDir,SAL_DIR_SEP,mSaveState[i].filename);
		//printf("In ScanSaveStates, mSaveState[%d].filename = %s\n", i, mSaveState[i].filename);
		//printf("In ScanSaveStates, mSaveState[%d].fullFilename = %s\n", i, mSaveState[i].fullFilename);
		if (sal_FileExists(mSaveState[i].fullFilename)==SAL_TRUE)
		{
			// we have a savestate
			mSaveState[i].inUse = 1;
		}
		else
		{
			// no save state
			mSaveState[i].inUse = 0;
		}
	}
	strcpy(mSaveStateName,romname);  // save the last scanned romname
}

static
bool8 LoadStateTemp()
{
	char name[SAL_MAX_PATH];
	bool8 ret;
	sprintf(name, "%s%s%s", sal_DirectoryGetTemp(), SAL_DIR_SEP, ".svt");
	if (!(ret = S9xUnfreezeGame(name)))
		fprintf(stderr, "Failed to read saved state at %s: %s\n", name, strerror(errno));
	return ret;
}

static
void SaveStateTemp()
{
	char name[SAL_MAX_PATH];
	sprintf(name, "%s%s%s", sal_DirectoryGetTemp(), SAL_DIR_SEP, ".svt");
	if (!S9xFreezeGame(name))
		fprintf(stderr, "Failed to write saved state at %s: %s\n", name, strerror(errno));
}

static
void DeleteStateTemp()
{
	char name[SAL_MAX_PATH];
	sprintf(name, "%s%s%s", sal_DirectoryGetTemp(), SAL_DIR_SEP, ".svt");
	sal_FileDelete(name);
}

bool LoadStateFile(s8 *filename)
{
	bool8 ret;
	if (!(ret = S9xUnfreezeGame(filename)))
		fprintf(stderr, "Failed to read saved state at %s: %s\n", filename, strerror(errno));
	return ret;
}

bool SaveStateFile(s8 *filename)
{
    bool8 ret;
	if (!(ret = S9xFreezeGame(filename)))
		fprintf(stderr, "Failed to write saved state at %s: %s\n", filename, strerror(errno));
    return ret;
}

u32 IsPreviewingState()
{
	return mPreviewingState;
}

static s32 SaveStateSelect(s32 mode)
{
	s8 text[128];
	s32 action=11;
	s32 saveno=0;
	u32 keys=0;
	u16 *pixTo,*pixFrom;

	if(mRomName[0]==0)
	{
		// no rom loaded
		// display error message and exit
		return(0);
	}
	// Allow the emulator to back out of loading a saved state for previewing.
	SaveStateTemp();
	ScanSaveStates(mRomName);
	sal_InputIgnore();

	while (action!=0&&action!=100)
	{
		keys=sal_InputPollRepeat();

		if(keys&SAL_INPUT_UP) {saveno--; action=1;}
		if(keys&SAL_INPUT_DOWN) {saveno++; action=1;}
		if(saveno<-1) saveno=9;
		if(saveno>9) saveno=-1;

		if(keys&INP_BUTTON_MENU_CANCEL) action=0; // exit
		else if((keys&INP_BUTTON_MENU_SELECT)&&(saveno==-1)) action=0; // exit
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==0)&&((action==2)||(action==5))) action=6;  // pre-save mode
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==1)&&(action==5)) action=8;  // pre-load mode
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==2)&&(action==5))
		{
			if(MenuMessageBox("Are you sure you want to delete","this save?","",MENU_MESSAGE_BOX_MODE_YESNO)==SAL_OK) action=13;  //delete slot with no preview
		}
		else if((keys&INP_BUTTON_MENU_PREVIEW_SAVESTATE)&&(action==12)) action=3;  // preview slot mode
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==1)&&(action==12)) action=8;  //load slot with no preview
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==0)&&(action==12)) action=6;  //save slot with no preview
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==2)&&(action==12))
		{
			if(MenuMessageBox("Are you sure you want to delete","this save?","",MENU_MESSAGE_BOX_MODE_YESNO)==SAL_OK) action=13;  //delete slot with no preview
		}

		PrintTitle("Save States");
		sal_VideoPrint(36,4,"UP/DOWN to choose a slot",SAL_RGB(31,8,8));

		if(saveno==-1)
		{
			if(action!=10&&action!=0)
			{
				action=10;
			}
		}
		else
		{
			sal_VideoDrawRect(0, 16, 262, 16, SAL_RGB(22,0,0));
			sprintf(text,"SLOT %d",saveno);
			sal_VideoPrint(107,20,text,SAL_RGB(31,31,31));
		}

		switch(action)
		{
			case 1:
				//sal_VideoPrint(112,145-36,14,"Checking....",(unsigned short)SAL_RGB(31,31,31));
				break;
			case 2:
				sal_VideoPrint(115,145-36,"FREE",SAL_RGB(31,31,31));
				break;
			case 3:
				sal_VideoPrint(75,145-36,"Previewing...",SAL_RGB(31,31,31));
				break;
			case 4:
				sal_VideoPrint(59,145-36,"Previewing failed",SAL_RGB(31,8,8));
				break;
			case 5:
			{
				u32 DestWidth = 205, DestHeight = 154;
				sal_VideoBitmapScale(0, 0, SNES_WIDTH, SNES_HEIGHT, DestWidth, DestHeight, SAL_SCREEN_WIDTH - DestWidth, &mTempFb[0], (u16*)sal_VideoGetBuffer()+(SAL_SCREEN_WIDTH*(((202 + 16) - DestHeight)/2))+((262 - DestWidth)/2));

				sal_VideoDrawRect(0, 186, 262, 16, SAL_RGB(22,0,0));

				if(mode==1) sal_VideoPrint((262-(strlen(MENU_TEXT_LOAD_SAVESTATE)<<3))>>1,190,MENU_TEXT_LOAD_SAVESTATE,SAL_RGB(31,31,31));
				else if(mode==0) sal_VideoPrint((262-(strlen(MENU_TEXT_OVERWRITE_SAVESTATE)<<3))>>1,190,MENU_TEXT_OVERWRITE_SAVESTATE,SAL_RGB(31,31,31));
				else if(mode==2) sal_VideoPrint((262-(strlen(MENU_TEXT_DELETE_SAVESTATE)<<3))>>1,190,MENU_TEXT_DELETE_SAVESTATE,SAL_RGB(31,31,31));
				break;
			}
			case 6:
				sal_VideoPrint(95,145-36,"Saving...",SAL_RGB(31,31,31));
				break;
			case 7:
				sal_VideoPrint(95,145-36,"Saving failed",SAL_RGB(31,8,8));
				break;
			case 8:
				sal_VideoPrint(87,145-36,"Loading...",SAL_RGB(31,31,31));
				break;
			case 9:
				sal_VideoPrint(87,145-36,"Loading failed",SAL_RGB(31,8,8));
				break;
			case 10:
				PrintBar(145-36-4);
				sal_VideoPrint(75,145-36,"Return to menu",SAL_RGB(31,31,31));
				break;
			case 12:
				sal_VideoPrint(95,145-36,"Slot used",SAL_RGB(31,31,31));
				sal_VideoPrint((262-(strlen(MENU_TEXT_PREVIEW_SAVESTATE)<<3))>>1,165,MENU_TEXT_PREVIEW_SAVESTATE,SAL_RGB(31,31,31));
				if(mode==1) sal_VideoPrint((262-(strlen(MENU_TEXT_LOAD_SAVESTATE)<<3))>>1,175,MENU_TEXT_LOAD_SAVESTATE,SAL_RGB(31,31,31));
				else if(mode==0) sal_VideoPrint((262-(strlen(MENU_TEXT_OVERWRITE_SAVESTATE)<<3))>>1,175,MENU_TEXT_OVERWRITE_SAVESTATE,SAL_RGB(31,31,31));
				else if(mode==2) sal_VideoPrint((262-(strlen(MENU_TEXT_DELETE_SAVESTATE)<<3))>>1,175,MENU_TEXT_DELETE_SAVESTATE,SAL_RGB(31,31,31));
				break;
			case 13:
				sal_VideoPrint(87,145-36,"Deleting...",SAL_RGB(31,31,31));
				break;
		}

		sal_VideoFlip(1);

		switch(action)
		{
			case 1:
				if(mSaveState[saveno].inUse)
				{
					action=3;
				}
				else
				{
					action=2;
				}
				break;
			case 3:
			{
				if (LoadStateFile(mSaveState[saveno].fullFilename))
				{
					// Loaded OK. Preview it by running the state for one frame.
					mPreviewingState = 1;
					sal_AudioSetMuted(1);
					GFX.Screen = (uint8 *) &mTempFb[0];
					IPPU.RenderThisFrame=TRUE;
					unsigned int fullScreenSave = mMenuOptions->fullScreen;
					mMenuOptions->fullScreen = 0;
					S9xMainLoop ();
					mMenuOptions->fullScreen = fullScreenSave;
					sal_AudioSetMuted(0);
					mPreviewingState = 0;
					action=5;
				}
				else
					action=4; // did not load correctly; report an error
				break;
			}
			case 6:
				//Reload state in case user has been previewing
				LoadStateTemp();
				SaveStateFile(mSaveState[saveno].fullFilename);
				mSaveState[saveno].inUse=1;
				action=1;
				break;
			case 7:
				action=1;
				break;
			case 8:
				if (LoadStateFile(mSaveState[saveno].fullFilename))
					action=100;  // loaded ok so exit
				else
					action=9; // did not load correctly; report an error
				break;
			case 9:
				action=1;
				break;
			case 11:
				action=1;
				break;
			case 13:
				sal_FileDelete(mSaveState[saveno].fullFilename);
				mSaveState[saveno].inUse = 0;
				action=1;
				break;
		}

		usleep(10000);
	}
	if (action!=100)
	{
		LoadStateTemp();
	}
	GFX.Screen = (uint8 *) sal_VideoGetBuffer();
	DeleteStateTemp();
	sal_InputIgnore();
	return(action);
}

static
void RenderMenu(const char *menuName, s32 menuCount, s32 menuSmooth, s32 menufocus)
{

	s32 i=0;
	u16 color=0;
	PrintTitle(menuName);

	for (i=0;i<menuCount;i++)
	{
		int x=0,y=0;

		y=(i<<4)-(menuSmooth>>4);
		x=8;
		y+=112 - 28;

		if (y<=48 - 28 || y>=232 - 36) continue;

		if (i==menufocus)
		{
			color=SAL_RGB(31,31,31);
			PrintBar(y-4);
		}
		else
		{
			color=SAL_RGB(31,31,31);
		}

		sal_VideoPrint(x,y,mMenuText[i],color);
	}
}

static
s32 SaveStateMenu(void)
{
	s32 menuExit=0,menuCount=SAVESTATE_MENU_COUNT,menufocus=0,menuSmooth=0;
	s32 action=0;
	s32 subaction=0;
	u32 keys=0;

	//Update
	strcpy(mMenuText[SAVESTATE_MENU_LOAD],"Load state");
	strcpy(mMenuText[SAVESTATE_MENU_SAVE],"Save state");
	strcpy(mMenuText[SAVESTATE_MENU_DELETE],"Delete state");
	strcpy(mMenuText[SAVESTATE_MENU_RETURN],"Back");
	sal_InputIgnore();

	while (!menuExit)
	{
		// Draw screen:
		menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
		RenderMenu("Save States", menuCount,menuSmooth,menufocus);
		sal_VideoFlip(1);

		keys=sal_InputPollRepeat();

		if (keys & INP_BUTTON_MENU_CANCEL)
		{
			while (keys)
			{
				// Draw screen:
				menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
				RenderMenu("Save States", menuCount,menuSmooth,menufocus);
				sal_VideoFlip(1);

				keys=sal_InputPoll();
			}

			menuExit=1;
		}
		else if (keys & INP_BUTTON_MENU_SELECT)
		{
			while (keys)
			{
				// Draw screen:
				menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
				RenderMenu("Save States", menuCount,menuSmooth,menufocus);
				sal_VideoFlip(1);

				keys=sal_InputPoll();
			}

			switch(menufocus)
			{
				case SAVESTATE_MENU_LOAD:
					subaction=SaveStateSelect(SAVESTATE_MODE_LOAD);
					if(subaction==100)
					{
						menuExit=1;
						action=100;
					}
					break;
				case SAVESTATE_MENU_SAVE:
					SaveStateSelect(SAVESTATE_MODE_SAVE);
					break;
				case SAVESTATE_MENU_DELETE:
					SaveStateSelect(SAVESTATE_MODE_DELETE);
					break;
				case SAVESTATE_MENU_RETURN:
					menuExit=1;
					break;
			}
		}
		else if ((keys & (SAL_INPUT_UP | SAL_INPUT_DOWN))
		      && (keys & (SAL_INPUT_UP | SAL_INPUT_DOWN)) != (SAL_INPUT_UP | SAL_INPUT_DOWN))
		{
			if (keys & SAL_INPUT_UP)
				menufocus--; // Up
			else if (keys & SAL_INPUT_DOWN)
				menufocus++; // Down

			if (menufocus>menuCount-1)
			{
				menufocus=0;
				menuSmooth=(menufocus<<8)-1;
			}
			else if (menufocus<0)
			{
				menufocus=menuCount-1;
				menuSmooth=(menufocus<<8)-1;
			}
		}

		usleep(10000);
	}
  sal_InputIgnore();
  return action;
}

void ShowCredits()
{
	s32 menuExit=0,menuCount=7,menufocus=0,menuSmooth=0;
	u32 keys=0;

	strcpy(mMenuText[0],"PocketSNES - built " __DATE__);
	strcpy(mMenuText[1],"-------------------------------------");
	strcpy(mMenuText[2],"Based on Snes9x version " VERSION /* snes9x.h */);
	strcpy(mMenuText[3],"PocketSNES created by Scott Ramsby");
	strcpy(mMenuText[4],"Initial port to the Dingoo by Reesy");
	strcpy(mMenuText[5],"Ported to OpenDingux by pcercuei");
	strcpy(mMenuText[6],"Optimisations and fixes by Nebuleon");

	sal_InputIgnore();
	while (!menuExit)
	{
		keys=sal_InputPollRepeat();

		if (keys & SAL_INPUT_UP) menufocus--; // Up
		if (keys & SAL_INPUT_DOWN) menufocus++; // Down


		if (keys&INP_BUTTON_MENU_CANCEL) menuExit=1;

		if (menufocus>menuCount-1)
		{
			menufocus=0;
			menuSmooth=(menufocus<<8)-1;
		}
		else if (menufocus<0)
		{
			menufocus=menuCount-1;
			menuSmooth=(menufocus<<8)-1;
		}

		// Draw screen:
		menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
		RenderMenu("Credits", menuCount,menuSmooth,menufocus);
		sal_VideoFlip(1);
		usleep(10000);
	}
	sal_InputIgnore();
}

static
void MainMenuUpdateText(s32 menu_index)
{
	switch(menu_index)
	{
		case MENU_STATE:
			strcpy(mMenuText[MENU_STATE],"Save states");
			break;

		case MENU_RESET_GAME:
			strcpy(mMenuText[MENU_RESET_GAME],"Reset game");
			break;

		case MENU_EXIT_APP:
			strcpy(mMenuText[MENU_EXIT_APP],"Exit PocketSNES");
			break;

		case MENU_CREDITS:
			strcpy(mMenuText[MENU_CREDITS],"Credits");
			break;

		case MENU_RETURN:
			strcpy(mMenuText[MENU_RETURN],"Return to game");
			break;

		case MENU_AUTO_SAVE_SRAM:
			sprintf(mMenuText[MENU_AUTO_SAVE_SRAM],
						"Save SRAM when changed:    %s",
						mMenuOptions->autoSaveSram ? " ON" : "OFF");
			break;

		case MENU_SOUND_SYNC:
			switch (mMenuOptions->soundSync)
			{
				case 0:
					strcpy(mMenuText[MENU_SOUND_SYNC], "Prefer fluid...          Video");
					break;
				case 1:
					strcpy(mMenuText[MENU_SOUND_SYNC], "Prefer fluid...      Vid & aud");
					break;
				default:
					strcpy(mMenuText[MENU_SOUND_SYNC], "Prefer fluid...          Audio");
					break;
			}

		case MENU_SOUND_ON:
			sprintf(mMenuText[MENU_SOUND_ON],
						"Sound:                     %s",
						mMenuOptions->soundEnabled ? " ON" : "OFF");
			break;

		case MENU_SOUND_RATE:
			sprintf(mMenuText[MENU_SOUND_RATE],"Sound rate:              %5d",mMenuOptions->soundRate);
			break;

		case MENU_SOUND_STEREO:
			sprintf(mMenuText[MENU_SOUND_STEREO],
						"Stereo:                    %s",
						mMenuOptions->stereo ? " ON" : "OFF");
			break;

#if 0
		case MENU_CPU_SPEED:
			sprintf(mMenuText[MENU_CPU_SPEED],"Cpu Speed:                  %d",mMenuOptions->cpuSpeed);
			break;

		case MENU_SOUND_VOL:
			sprintf(mMenuText[MENU_SOUND_VOL],"Volume:                     %d",mMenuOptions->volume);
			break;
#endif

		case MENU_FRAMESKIP:
			switch(mMenuOptions->frameSkip)
			{
				case 0:
					strcpy(mMenuText[MENU_FRAMESKIP],
						"Frameskip:                AUTO");
					break;
				default:
					sprintf(mMenuText[MENU_FRAMESKIP],
						"Frameskip:                   %1d",mMenuOptions->frameSkip-1);
					break;
			}
			break;

		case MENU_FPS:
			switch(mMenuOptions->showFps)
			{
				case 0:
					strcpy(mMenuText[MENU_FPS],"Show FPS:                  OFF");
					break;
				case 1:
					strcpy(mMenuText[MENU_FPS],"Show FPS:                   ON");
					break;
			}
			break;

		case MENU_FULLSCREEN:
			switch(mMenuOptions->fullScreen)
			{
				case 0:
					strcpy(mMenuText[MENU_FULLSCREEN],"Full screen:               OFF");
					break;
				case 1:
					strcpy(mMenuText[MENU_FULLSCREEN],"Full screen:              FAST");
					break;
				case 2:
					strcpy(mMenuText[MENU_FULLSCREEN],"Full screen:            SMOOTH");
					break;
				case 3:
					strcpy(mMenuText[MENU_FULLSCREEN],"Full screen:          HARDWARE");
					break;
			}
			break;

		case MENU_LOAD_GLOBAL_SETTINGS:
			strcpy(mMenuText[MENU_LOAD_GLOBAL_SETTINGS],"Load global settings");
			break;

		case MENU_SAVE_GLOBAL_SETTINGS:
			strcpy(mMenuText[MENU_SAVE_GLOBAL_SETTINGS],"Save global settings");
			break;

		case MENU_LOAD_CURRENT_SETTINGS:
			strcpy(mMenuText[MENU_LOAD_CURRENT_SETTINGS],"Load per-game settings");
			break;

		case MENU_SAVE_CURRENT_SETTINGS:
			strcpy(mMenuText[MENU_SAVE_CURRENT_SETTINGS],"Save per-game settings");
			break;

		case MENU_DELETE_CURRENT_SETTINGS:
			strcpy(mMenuText[MENU_DELETE_CURRENT_SETTINGS],"Delete per-game settings");
			break;

		case MENU_SAVE_SRAM:
			strcpy(mMenuText[MENU_SAVE_SRAM],"Save SRAM");
			break;

#ifndef NO_ROM_BROWSER
		case MENU_ROM_SELECT:
			strcpy(mMenuText[MENU_ROM_SELECT],"Select ROM");
			break;
#endif
	}
}

static
void MainMenuUpdateTextAll(void)
{
	MainMenuUpdateText(MENU_STATE);
	MainMenuUpdateText(MENU_RESET_GAME);
	MainMenuUpdateText(MENU_EXIT_APP);
	MainMenuUpdateText(MENU_RETURN);
//	MainMenuUpdateText(MENU_CPU_SPEED);
	MainMenuUpdateText(MENU_SOUND_ON);
	MainMenuUpdateText(MENU_SOUND_STEREO);
	MainMenuUpdateText(MENU_SOUND_RATE);
//	MainMenuUpdateText(MENU_SOUND_VOL);
	MainMenuUpdateText(MENU_FRAMESKIP);
	MainMenuUpdateText(MENU_FPS);
	MainMenuUpdateText(MENU_SOUND_SYNC);
	MainMenuUpdateText(MENU_FULLSCREEN);
	MainMenuUpdateText(MENU_LOAD_GLOBAL_SETTINGS);
	MainMenuUpdateText(MENU_SAVE_GLOBAL_SETTINGS);
	MainMenuUpdateText(MENU_LOAD_CURRENT_SETTINGS);
	MainMenuUpdateText(MENU_SAVE_CURRENT_SETTINGS);
	MainMenuUpdateText(MENU_DELETE_CURRENT_SETTINGS);
	MainMenuUpdateText(MENU_RETURN);
	MainMenuUpdateText(MENU_CREDITS);
	MainMenuUpdateText(MENU_AUTO_SAVE_SRAM);
	MainMenuUpdateText(MENU_SAVE_SRAM);
#ifndef NO_ROM_BROWSER
	MainMenuUpdateText(MENU_ROM_SELECT);
#endif
}

void MenuReloadOptions()
{
	if(mRomName[0]!=0)
	{
		//Load settings for game
		if (LoadMenuOptions(mSystemDir, mRomName, MENU_OPTIONS_EXT, (s8*)mMenuOptions, sizeof(struct MENU_OPTIONS), 0) == SAL_OK)
		{
			return;
		}
	}

	//Load global settings
	if(LoadMenuOptions(mSystemDir, MENU_OPTIONS_FILENAME, MENU_OPTIONS_EXT, (s8*)mMenuOptions, sizeof(struct MENU_OPTIONS), 0) == SAL_OK)
	{
		return;
	}

	DefaultMenuOptions();
}

void MenuInit(const char *systemDir, struct MENU_OPTIONS *menuOptions, char *romName)
{
	s8 filename[SAL_MAX_PATH];
	u16 *pix;
	s32 x;

	//strcpy(mSystemDir,systemDir);
	strcpy(mSystemDir,mRomPath);
	//printf("******* %s\n", mSystemDir);
	mMenuOptions=menuOptions;

	if(LoadMenuOptions(mSystemDir, DEFAULT_ROM_DIR_FILENAME, DEFAULT_ROM_DIR_EXT, mRomDir, SAL_MAX_PATH, 0)!=SAL_OK)
	{
		strcpy(mRomDir,systemDir);
	}

	pix=&mMenuBackground[0];
	for(x=0;x<SAL_SCREEN_WIDTH * SAL_SCREEN_HEIGHT;x++) *pix++=SAL_RGB(0,0,0);

	sal_ImageLoad("pocketsnes_bg.png", &mMenuBackground, SAL_SCREEN_WIDTH, SAL_SCREEN_HEIGHT);

	MenuReloadOptions();

	/// ------ Load save states -------
	strcpy(mRomName, romName);
	ScanSaveStates(mRomName);
}




s32 MenuRun(s8 *romName)
{
	s32 menuExit=0,menuCount=MENU_COUNT,menufocus=0,menuSmooth=0;
	s32 action=EVENT_NONE;
	s32 subaction=0;
	u32 keys=0;

	sal_CpuSpeedSet(MENU_NORMAL_CPU_SPEED);

	if(sal_StringCompare(mRomName,romName)!=0)
	{
		action=EVENT_LOAD_ROM;
		strcpy(mRomName,romName);
		return action;
	}

#if 0
	if((mMenuOptions->autoSaveSram) && (mRomName[0]!=0))
	{
		MenuMessageBox("Saving SRAM...","","",MENU_MESSAGE_BOX_MODE_MSG);
		S9xSaveSRAM(0);
	}
#endif

	MainMenuUpdateTextAll();
	sal_InputIgnore();

	while (!menuExit)
	{
		// Draw screen:
		menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
		RenderMenu("Main Menu", menuCount,menuSmooth,menufocus);
		sal_VideoFlip(1);

		keys=sal_InputPollRepeat();

		if(keys & INP_BUTTON_MENU_EXIT)
		{
			action = EVENT_EXIT_APP;
			menuExit=1;
			break;
		}

		if (keys & INP_BUTTON_MENU_SELECT)
		{
			while (keys)
			{
				// Draw screen:
				menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
				RenderMenu("Main Menu", menuCount,menuSmooth,menufocus);
				sal_VideoFlip(1);

				keys=sal_InputPoll();

				usleep(10000);
			}

			switch(menufocus)
			{
#ifndef NO_ROM_BROWSER
				case MENU_ROM_SELECT:
					subaction=FileSelect();
					if (subaction==1)
					{
						action=EVENT_LOAD_ROM;
						strcpy(romName,mRomName);
						MenuReloadOptions();
						menuExit=1;
					}
					break;
#endif
				case MENU_LOAD_GLOBAL_SETTINGS:
					LoadMenuOptions(mSystemDir, MENU_OPTIONS_FILENAME, MENU_OPTIONS_EXT, (char*)mMenuOptions, sizeof(struct MENU_OPTIONS), 1);
					MainMenuUpdateTextAll();
					break;
				case MENU_SAVE_GLOBAL_SETTINGS:
					SaveMenuOptions(mSystemDir, MENU_OPTIONS_FILENAME, MENU_OPTIONS_EXT, (char*)mMenuOptions, sizeof(struct MENU_OPTIONS), 1);
					break;

				case MENU_LOAD_CURRENT_SETTINGS:
					if(mRomName[0]!=0)
					{
						LoadMenuOptions(mSystemDir, mRomName, MENU_OPTIONS_EXT, (char*)mMenuOptions, sizeof(struct MENU_OPTIONS), 1);

						MainMenuUpdateTextAll();
					}
					break;
				case MENU_SAVE_CURRENT_SETTINGS:
					if(mRomName[0]!=0)
					{
						SaveMenuOptions(mSystemDir, mRomName, MENU_OPTIONS_EXT, (char*)mMenuOptions, sizeof(struct MENU_OPTIONS), 1);
					}
					break;

				case MENU_DELETE_CURRENT_SETTINGS:
					if(mRomName[0]!=0)
					{
						DeleteMenuOptions(mSystemDir, mRomName, MENU_OPTIONS_EXT, 1);
					}
					break;

				case MENU_STATE:
					if(mRomName[0]!=0)
					{
						subaction=SaveStateMenu();
						if (subaction==100)
						{
							action=EVENT_RUN_ROM;
							menuExit=1;
						}
					}
					MainMenuUpdateTextAll();
					break;

				case MENU_SAVE_SRAM:
					if(mRomName[0]!=0)
					{
						MenuMessageBox("","","Saving SRAM...",MENU_MESSAGE_BOX_MODE_MSG);
						S9xSaveSRAM(1);
					}
					break;

				case MENU_CREDITS:
					ShowCredits();
					MainMenuUpdateTextAll();
					break;

				case MENU_RESET_GAME:
					if(mRomName[0]!=0)
					{
						action=EVENT_RESET_ROM;
						menuExit=1;
					}
					break;
				case MENU_RETURN:
					if(mRomName[0]!=0)
					{
						action=EVENT_RUN_ROM;
						menuExit=1;
					}
					break;
				case MENU_EXIT_APP:
					action=EVENT_EXIT_APP;
					menuExit=1;
					break;

			}
		}
		else if (keys & INP_BUTTON_MENU_CANCEL)
		{
			if(mRomName[0]!=0)
			{
				action=EVENT_RUN_ROM;
				menuExit=1;
			}
		}
		else if ((keys & (SAL_INPUT_UP | SAL_INPUT_DOWN))
		      && (keys & (SAL_INPUT_UP | SAL_INPUT_DOWN)) != (SAL_INPUT_UP | SAL_INPUT_DOWN))
		{
			if (keys & SAL_INPUT_UP)
				menufocus--; // Up
			else if (keys & SAL_INPUT_DOWN)
				menufocus++; // Down

			if (menufocus>menuCount-1)
			{
				menufocus=0;
				menuSmooth=(menufocus<<8)-1;
			}
			else if (menufocus<0)
			{
				menufocus=menuCount-1;
				menuSmooth=(menufocus<<8)-1;
			}
		}
		else if ((keys & (SAL_INPUT_LEFT | SAL_INPUT_RIGHT))
		      && (keys & (SAL_INPUT_LEFT | SAL_INPUT_RIGHT)) != (SAL_INPUT_LEFT | SAL_INPUT_RIGHT))
		{
			switch(menufocus)
			{
				case MENU_SOUND_ON:
					mMenuOptions->soundEnabled^=1;
					MainMenuUpdateText(MENU_SOUND_ON);
					break;

				case MENU_SOUND_STEREO:
					mMenuOptions->stereo^=1;
					MainMenuUpdateText(MENU_SOUND_STEREO);
					break;

				case MENU_AUTO_SAVE_SRAM:
					mMenuOptions->autoSaveSram^=1;
					MainMenuUpdateText(MENU_AUTO_SAVE_SRAM);
					break;

				case MENU_SOUND_SYNC:
					if (keys & SAL_INPUT_RIGHT)
					{
						mMenuOptions->soundSync = (mMenuOptions->soundSync + 1) % 3;
					}
					else
					{
						if (mMenuOptions->soundSync == 0)
							mMenuOptions->soundSync = 2;
						else
							mMenuOptions->soundSync--;
					}
					MainMenuUpdateText(MENU_SOUND_SYNC);
					break;

#if 0
				case MENU_CPU_SPEED:

					if (keys & SAL_INPUT_RIGHT)
					{
						if(keys&INP_BUTTON_MENU_SELECT)
						{
							mMenuOptions->cpuSpeed=sal_CpuSpeedNextFast(mMenuOptions->cpuSpeed);
						}
						else
						{
							mMenuOptions->cpuSpeed=sal_CpuSpeedNext(mMenuOptions->cpuSpeed);
						}
					}
					else
					{
						if(keys&INP_BUTTON_MENU_SELECT)
						{
							mMenuOptions->cpuSpeed=sal_CpuSpeedPreviousFast(mMenuOptions->cpuSpeed);
						}
						else
						{
							mMenuOptions->cpuSpeed=sal_CpuSpeedPrevious(mMenuOptions->cpuSpeed);
						}
					}
					MainMenuUpdateText(MENU_CPU_SPEED);
					break;
#endif

				case MENU_SOUND_RATE:
					if (keys & SAL_INPUT_RIGHT)
					{
						mMenuOptions->soundRate=sal_AudioRateNext(mMenuOptions->soundRate);
					}
					else
					{
						mMenuOptions->soundRate=sal_AudioRatePrevious(mMenuOptions->soundRate);
					}
					MainMenuUpdateText(MENU_SOUND_RATE);
					break;

#if 0
				case MENU_SOUND_VOL:
					if (keys & SAL_INPUT_RIGHT)
					{
						mMenuOptions->volume+=1;
						if(mMenuOptions->volume>31) mMenuOptions->volume=0;
					}
					else
					{
						mMenuOptions->volume-=1;
						if(mMenuOptions->volume>31) mMenuOptions->volume=31;

					}
					MainMenuUpdateText(MENU_SOUND_VOL);
					break;
#endif

				case MENU_FRAMESKIP:
					if (keys & SAL_INPUT_RIGHT)
					{
						mMenuOptions->frameSkip++;
						if(mMenuOptions->frameSkip>6) mMenuOptions->frameSkip=0;
					}
					else
					{
						mMenuOptions->frameSkip--;
						if(mMenuOptions->frameSkip>6) mMenuOptions->frameSkip=6;
					}
					MainMenuUpdateText(MENU_FRAMESKIP);
					break;

				case MENU_FPS:
					mMenuOptions->showFps^=1;
					MainMenuUpdateText(MENU_FPS);
					break;

				case MENU_FULLSCREEN:
					if (keys & SAL_INPUT_RIGHT)
					{
						mMenuOptions->fullScreen = (mMenuOptions->fullScreen + 1) % 4;
					}
					else
					{
						if (mMenuOptions->fullScreen == 0)
							mMenuOptions->fullScreen = 3;
						else
							mMenuOptions->fullScreen--;
					}
					MainMenuUpdateText(MENU_FULLSCREEN);
					break;
			}
		}

		usleep(10000);
	}

  sal_InputWaitForRelease();

  return action;
}
