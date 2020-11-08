
#include <stdio.h>
#include <dirent.h>
#include <SDL.h>
#include <SDL/SDL_ttf.h>
#include <sys/time.h>
#include "sal.h"
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#define PALETTE_BUFFER_LENGTH	256*2*4
//#define BLACKER_BLACKS

#define RES_HW_SCREEN_HORIZONTAL  240
#define RES_HW_SCREEN_VERTICAL    240

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ABS(x) (((x) < 0) ? (-x) : (x))



#define AVERAGE(z, x) ((((z) & 0xF7DEF7DE) >> 1) + (((x) & 0xF7DEF7DE) >> 1))
#define AVERAGEHI(AB) ((((AB) & 0xF7DE0000) >> 1) + (((AB) & 0xF7DE) << 15))
#define AVERAGELO(CD) ((((CD) & 0xF7DE) >> 1) + (((CD) & 0xF7DE0000) >> 17))

// Support math
#define Half(A) (((A) >> 1) & 0x7BEF)
#define Quarter(A) (((A) >> 2) & 0x39E7)
// Error correction expressions to piece back the lower bits together
#define RestHalf(A) ((A) & 0x0821)
#define RestQuarter(A) ((A) & 0x1863)

// Error correction expressions for quarters of pixels
#define Corr1_3(A, B)     Quarter(RestQuarter(A) + (RestHalf(B) << 1) + RestQuarter(B))
#define Corr3_1(A, B)     Quarter((RestHalf(A) << 1) + RestQuarter(A) + RestQuarter(B))

// Error correction expressions for halves
#define Corr1_1(A, B)     ((A) & (B) & 0x0821)

// Quarters
#define Weight1_3(A, B)   (Quarter(A) + Half(B) + Quarter(B) + Corr1_3(A, B))
#define Weight3_1(A, B)   (Half(A) + Quarter(A) + Quarter(B) + Corr3_1(A, B))

// Halves
#define Weight1_1(A, B)   (Half(A) + Half(B) + Corr1_1(A, B))

SDL_Surface *hw_screen = NULL;
SDL_Surface *virtual_hw_screen = NULL;

static u32 mSoundThreadFlag=0;
static u32 mSoundLastCpuSpeed=0;
static u32 mPaletteBuffer[PALETTE_BUFFER_LENGTH];
static u32 *mPaletteCurr=(u32*)&mPaletteBuffer[0];
static u32 *mPaletteLast=(u32*)&mPaletteBuffer[0];
static u32 *mPaletteEnd=(u32*)&mPaletteBuffer[PALETTE_BUFFER_LENGTH];
static u32 mInputFirst=0;

s32 mCpuSpeedLookup[1]={0};

#include <sal_common.h>

#define CASE(sym, key) \
  case SDLK_##sym: \
	/*printf("Event: %s\n", #sym);*/ \
	inputHeld &= ~(SAL_INPUT_##key); \
	inputHeld |= type << SAL_INPUT_INDEX_##key; \
	break

static u32 inputHeld = 0;

static u32 sal_Input(int held)
{
#if 1
	SDL_Event event;
	int i=0;
	u32 timer=0;

	if (!SDL_PollEvent(&event)) {
		if (held)
			return inputHeld;
		return 0;
	}

	if(event.type == SDL_QUIT){
		printf("\nWhat? How dare you interrupt me ?\n");
		mExit = 1;
	}

	Uint8 type = (event.key.state == SDL_PRESSED);

	switch(event.key.keysym.sym) {
		CASE(a, A);
		CASE(b, B);
		CASE(x, X);
		CASE(y, Y);
		CASE(m, L);
		CASE(n, R);
		CASE(s, START);
		CASE(k, SELECT);
		CASE(u, UP);
		CASE(d, DOWN);
		CASE(l, LEFT);
		CASE(r, RIGHT);
		CASE(q, MENU);
		CASE(h, ASPECT_RATIO);
		//CASE(e, EXIT);
		default: break;
	}

	mInputRepeat = inputHeld;

#else
	int i=0;
	u32 inputHeld=0;
	u32 timer=0;
	u8 *keystate;

	SDL_PumpEvents();

	keystate = SDL_GetKeyState(NULL);

	if ( keystate[SDLK_i] ) inputHeld|=SAL_INPUT_A;
	if ( keystate[SDLK_j] ) inputHeld|=SAL_INPUT_B;
	if ( keystate[SDLK_u] ) inputHeld|=SAL_INPUT_X;
	if ( keystate[SDLK_h] ) inputHeld|=SAL_INPUT_Y;
	if ( keystate[SDLK_o] ) inputHeld|=SAL_INPUT_L;
	if ( keystate[SDLK_k] ) inputHeld|=SAL_INPUT_R;
	if ( keystate[SDLK_v] ) inputHeld|=SAL_INPUT_START;
	if ( keystate[SDLK_c] ) inputHeld|=SAL_INPUT_SELECT;
	if ( keystate[SDLK_UP] ) inputHeld|=SAL_INPUT_UP;
	if ( keystate[SDLK_DOWN] ) inputHeld|=SAL_INPUT_DOWN;
	if ( keystate[SDLK_LEFT] ) inputHeld|=SAL_INPUT_LEFT;
	if ( keystate[SDLK_RIGHT] ) inputHeld|=SAL_INPUT_RIGHT;

	// Process key repeats
	timer=sal_TimerRead();
	for (i=0;i<32;i++)
	{
		if (inputHeld&(1<<i))
		{
			if(mInputFirst&(1<<i))
			{
				if (mInputRepeatTimer[i]<timer)
				{
					mInputRepeat|=1<<i;
					mInputRepeatTimer[i]=timer+10;
				}
				else
				{
					mInputRepeat&=~(1<<i);
				}
			}
			else
			{
				//First press of button
				//set timer to expire later than usual
				mInputFirst|=(1<<i);
				mInputRepeat|=1<<i;
				mInputRepeatTimer[i]=timer+50;
			}
		}
		else
		{
			mInputRepeatTimer[i]=timer-10;
			mInputRepeat&=~(1<<i);
			mInputFirst&=~(1<<i);
		}

	}

	if(mInputIgnore)
	{
		//A request to ignore all key presses until all keys have been released has been made
		//check for release and clear flag, otherwise clear inputHeld and mInputRepeat
		if (inputHeld == 0)
		{
			mInputIgnore=0;
		}
		inputHeld=0;
		mInputRepeat=0;
	}
#endif

	return inputHeld;
}

static int key_repeat_enabled = 1;

void sal_force_no_menu_detection(){
	inputHeld &= ~(SAL_INPUT_MENU);
	//inputHeld |= 0 << SAL_INPUT_INDEX_MENU;
}

u32 sal_InputPollRepeat()
{
	if (!key_repeat_enabled) {
		SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
		key_repeat_enabled = 1;
	}
	return sal_Input(0);
}

u32 sal_InputPoll()
{
	if (key_repeat_enabled) {
		SDL_EnableKeyRepeat(0, 0);
		key_repeat_enabled = 0;
	}
	return sal_Input(1);
}

const char* sal_DirectoryGetTemp(void)
{
	return "/tmp";
}

void sal_CpuSpeedSet(u32 mhz)
{

}

u32 sal_CpuSpeedNext(u32 currSpeed)
{
	u32 newSpeed=currSpeed+1;
	if(newSpeed > 500) newSpeed = 500;
	return newSpeed;
}

u32 sal_CpuSpeedPrevious(u32 currSpeed)
{
	u32 newSpeed=currSpeed-1;
	if(newSpeed > 500) newSpeed = 0;
	return newSpeed;
}

u32 sal_CpuSpeedNextFast(u32 currSpeed)
{
	u32 newSpeed=currSpeed+10;
	if(newSpeed > 500) newSpeed = 500;
	return newSpeed;
}

u32 sal_CpuSpeedPreviousFast(u32 currSpeed)
{
	u32 newSpeed=currSpeed-10;
	if(newSpeed > 500) newSpeed = 0;
	return newSpeed;
}

s32 sal_Init(void)
{
	if( SDL_Init( SDL_INIT_EVERYTHING ) == -1 )
	//if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) == -1 )
	{
		return SAL_ERROR;
	}

	if(TTF_Init())
	{
        fprintf(stderr, "Error TTF_Init: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
	}

	sal_TimerInit(60);

	memset(mInputRepeatTimer,0,sizeof(mInputRepeatTimer));

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	return SAL_OK;
}

static int fbfd;
u32 sal_VideoInit(u32 bpp)
{

	mBpp=bpp;

	/*SDL_VideoInfo * vidInfo = SDL_GetVideoInfo();
	printf("Vids infos:\n");
	printf("hw_available:%d\n", vidInfo->hw_available);
	printf("wm_available:%d\n", vidInfo->wm_available);
	printf("blit_hw:%d\n", vidInfo->blit_hw);
	printf("blit_hw_CC:%d\n", vidInfo->blit_hw_CC);
	printf("blit_sw:%d\n", vidInfo->blit_sw);
	printf("blit_sw_A:%d\n", vidInfo->blit_sw_A);
	printf("blit_fill:%d\n", vidInfo->blit_fill);
	printf("video_mem:%d\n", vidInfo->video_mem);
	printf("BitsPerPixel:%d\n", vidInfo->vfmt->BitsPerPixel);
	printf("BytesPerPixel:%d\n", vidInfo->vfmt->BytesPerPixel);
	printf("BytesPerPixel:%d\n", vidInfo->vfmt->BytesPerPixel);


	/* Open the file for reading and writing */
    /*fbfd = open("/dev/fb0", O_RDWR);
	if (!fbfd) {
                printf("Error: cannot open framebuffer device.\n");
                exit(1);
        }*/



	//Set up the screen
	/*hw_screen = SDL_SetVideoMode(RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL,
                                16, SDL_FULLSCREEN | SDL_HWSURFACE );*/
	/*hw_screen = SDL_SetVideoMode(RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL,
                                16, SDL_HWSURFACE);*/
	hw_screen = SDL_SetVideoMode(RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL,
                                16, SDL_HWSURFACE | SDL_DOUBLEBUF);
    //If there was an error in setting up the screen
    if( hw_screen == NULL )
    {
		sal_LastErrorSet("SDL_SetVideoMode failed");
		return SAL_ERROR;
    }

	if(SDL_GetVideoSurface()->flags & SDL_HWSURFACE ){
		printf("hard\n");
	}
	else{
		printf("soft\n");
	}



    virtual_hw_screen = SDL_CreateRGBSurface(SDL_SWSURFACE,
	RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL, 16, 0xFFFF, 0xFFFF, 0xFFFF, 0);
    if( virtual_hw_screen == NULL )
    {
		sal_LastErrorSet("Vound not create virtual_hw_screen");
		return SAL_ERROR;
    }

	SDL_ShowCursor(0);

	// Set window name
	//SDL_WM_SetCaption("Game", NULL);

    // lock surface if needed
	/*if (SDL_MUSTLOCK(hw_screen))
	{
		if (SDL_LockSurface(hw_screen) < 0)
		{
			sal_LastErrorSet("unable to lock surface");
			return SAL_ERROR;
		}
	}*/

	return SAL_OK;
}

u32 sal_VideoGetWidth()
{
	return hw_screen->w;
}

u32 sal_VideoGetHeight()
{
	return hw_screen->h;
}

u32 sal_VideoGetPitch()
{
	return hw_screen->pitch;
}

void sal_VideoEnterGame(u32 fullscreenOption, u32 pal, u32 refreshRate)
{
#if 0
#ifdef GCW_ZERO
	/* Copied from C++ headers which we can't include in C */
	unsigned int Width = 256 /* SNES_WIDTH */,
	             Height = pal ? 239 /* SNES_HEIGHT_EXTENDED */ : 224 /* SNES_HEIGHT */;
	if (fullscreenOption != 3)
	{
		Width = SAL_SCREEN_WIDTH;
		Height = SAL_SCREEN_HEIGHT;
	}
	if (SDL_MUSTLOCK(hw_screen))
		SDL_UnlockSurface(hw_screen);
	hw_screen = SDL_SetVideoMode(Width, Height, mBpp, SDL_HWSURFACE |
#ifdef SDL_TRIPLEBUF
		SDL_TRIPLEBUF
#else
		SDL_DOUBLEBUF
#endif
		);
	mRefreshRate = refreshRate;
	if (SDL_MUSTLOCK(hw_screen))
		SDL_LockSurface(hw_screen);
#endif
#endif
}

void sal_VideoSetPAL(u32 fullscreenOption, u32 pal)
{
	if (fullscreenOption == 3) /* hardware scaling */
	{
		sal_VideoEnterGame(fullscreenOption, pal, mRefreshRate);
	}
}

void sal_VideoExitGame()
{
#if 0
#ifdef GCW_ZERO
	if (SDL_MUSTLOCK(hw_screen))
		SDL_UnlockSurface(hw_screen);
	hw_screen = SDL_SetVideoMode(SAL_SCREEN_WIDTH, SAL_SCREEN_HEIGHT, mBpp, SDL_HWSURFACE | SDL_DOUBLEBUF);
	if (SDL_MUSTLOCK(hw_screen))
		SDL_LockSurface(hw_screen);
#endif
#endif
}

void sal_VideoBitmapDim(u16* img, u32 pixelCount)
{
	u32 i;
	for (i = 0; i < pixelCount; i += 2)
		*(u32 *) &img[i] = (*(u32 *) &img[i] & 0xF7DEF7DE) >> 1;
	if (pixelCount & 1)
		img[i - 1] = (img[i - 1] & 0xF7DE) >> 1;
}

void SDL_Rotate_270(SDL_Surface * src_surface, SDL_Surface * dst_surface){
	int i, j;


	static int prev_time = 0;
	if(SDL_GetTicks() - prev_time < 1000/30){
		return;
	}
	prev_time = SDL_GetTicks();


	/// --- Unlock surfaces and get pixels ---
	if (SDL_MUSTLOCK(src_surface)) {
		SDL_UnlockSurface(src_surface);
	}
	if (SDL_MUSTLOCK(dst_surface)) {
		SDL_UnlockSurface(dst_surface);
	}
    uint16_t *source_pixels = (uint16_t*) src_surface->pixels;
    uint16_t *dest_pixels = (uint16_t*) dst_surface->pixels;

    /// --- Checking for right pixel format ---
    if(src_surface->format->BitsPerPixel != 16){
	printf("Error in SDL_FastBlit, Wrong src_surface pixel format: %d bpb, expected: 16 bpb\n", src_surface->format->BitsPerPixel);
	return;
    }
    if(dst_surface->format->BitsPerPixel != 16){
	printf("Error in SDL_FastBlit, Wrong dst_surface pixel format: %d bpb, expected: 16 bpb\n", dst_surface->format->BitsPerPixel);
	return;
    }

    /// --- Checking if same dimensions ---
    if(dst_surface->w != src_surface->w || dst_surface->h != src_surface->h){
	printf("Error in SDL_FastBlit, dst_surface (%dx%d) and src_surface (%dx%d) have different dimensions\n",
	       dst_surface->w, dst_surface->h, src_surface->w, src_surface->h);
	return;
    }

	/// --- Pixel copy and rotation (270) ---
	uint16_t *cur_p_src, *cur_p_dst;
	for(i=0; i<src_surface->h; i++){
	for(j=0; j<src_surface->w; j++){
	cur_p_src = source_pixels + i*src_surface->w + j;
	cur_p_dst = dest_pixels + (dst_surface->h-1-j)*dst_surface->w + i;
	*cur_p_dst = *cur_p_src;
	}
	}

	/// --- Lock surfaces ---
	if (SDL_MUSTLOCK(src_surface)) {
		SDL_LockSurface(src_surface);
	}
	if (SDL_MUSTLOCK(dst_surface)) {
		SDL_LockSurface(dst_surface);
	}
}

void SDL_Rotate_270_StandardSurfaces(){
	//SDL_Rotate_270(virtual_hw_screen, hw_screen);

	//SDL_BlitSurface(virtual_hw_screen, NULL, hw_screen, NULL);
	//clear_screen((uint16_t*) hw_screen->pixels, 240, 240, 0);

	memcpy(hw_screen->pixels, virtual_hw_screen->pixels, hw_screen->w*hw_screen->h*sizeof(uint16_t));

#if 0
	if (SDL_MUSTLOCK(hw_screen)) {
		SDL_UnlockSurface(hw_screen);
	}

	//memset(hw_screen->pixels, 0x73, 240*240*2);


	/*static uint8_t add = 1;
	int i, j;
	for(i=0; i<hw_screen->h/2; i++){
	for(j=0; j<hw_screen->w; j++){
		*((uint16_t*)hw_screen->pixels+(i*2+add)*hw_screen->w+j) =
		*((uint16_t*)virtual_hw_screen->pixels+(i*2+add)*hw_screen->w+j);
	}
	}
	add = (add+1)%2;*/

	if (SDL_MUSTLOCK(hw_screen)) {
		SDL_LockSurface(hw_screen);
	}
#endif

	/*int i;
	for(i=0; i<hw_screen->h/2; i++){

		write(fbfd,buffer,n); *((uint16_t*)virtual_hw_screen->pixels+(i*2+add)*hw_screen->w+j);
	}*/

	/*// Rewind file
	lseek(fbfd, 0, SEEK_SET);
	write(fbfd,virtual_hw_screen->pixels,240*240*2);*/

}

void sal_VideoFlip(s32 vsync)
{
	/*if (SDL_MUSTLOCK(hw_screen)) {
		SDL_UnlockSurface(hw_screen);
		SDL_Flip(hw_screen);
		SDL_LockSurface(hw_screen);
	} else{
		SDL_Flip(hw_screen);
	}*/

	SDL_Flip(hw_screen);

	/*if(SDL_GetVideoSurface()->flags & SDL_HWSURFACE ){
	printf("hard\n");
	}
	else{
	printf("soft\n");
	}*/

	//SDL_Flip(hw_screen);
}

void sal_VideoLock()
{
	/*if (SDL_MUSTLOCK(hw_screen)) {
		SDL_LockSurface(hw_screen);
	}*/
}

void sal_VideoUnlock()
{
	/*if (SDL_MUSTLOCK(hw_screen)) {
		SDL_UnlockSurface(hw_screen);
	}*/
}



/*void sal_VideoRotateAndFlip(uint32_t fps){
	if (!fps) fps=25;

	static int prev_time = 0;
	if(SDL_GetTicks() - prev_time < 1000/fps){
		return;
	}
	prev_time = SDL_GetTicks();

	SDL_Rotate_270(virtual_hw_screen, hw_screen);
	sal_VideoFlip(0);
}*/

void *sal_VideoGetBuffer()
{
	return (void*)hw_screen->pixels;
}

void *sal_VirtualVideoGetBuffer()
{
	return (void*)virtual_hw_screen->pixels;
}

void sal_VideoPaletteSync()
{

}

void sal_VideoPaletteSet(u32 index, u32 color)
{
	*mPaletteCurr++=index;
	*mPaletteCurr++=color;
	if(mPaletteCurr>mPaletteEnd) mPaletteCurr=&mPaletteBuffer[0];
}

void sal_Reset(void)
{

	//close(fbfd);

	SDL_FreeSurface(virtual_hw_screen);
	TTF_Quit();
	sal_AudioClose();
	SDL_Quit();
}

int mainEntry(int argc, char *argv[]);

// Prove entry point wrapper
int main(int argc, char *argv[])
{
	return mainEntry(argc,argv);
//	return mainEntry(argc-1,&argv[1]);
}




/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_NNOptimized_AllowOutOfScreen(uint16_t *src_screen, uint16_t *dst_screen,
								int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  int x2, y2 ;

  /// --- Compute padding for centering when out of bounds ---
  int y_padding = (RES_HW_SCREEN_VERTICAL-dst_h)/2;
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;
  //printf("virtual_screen->h=%d, h2=%d\n", virtual_screen->h, h2);

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    uint16_t* t = (uint16_t*)(dst_screen + (i+y_padding)*
	((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    y2 = ((i*y_ratio)>>16);
    uint16_t* p = (uint16_t*)(src_screen + (y2*w1 + x_padding_ratio) );
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      x2 = (rat>>16);
#ifdef BLACKER_BLACKS
      *t++ = p[x2] & 0xFFDF; /// Optimization for blacker blacks
#else
      *t++ = p[x2]; /// Optimization for blacker blacks
#endif
      rat += x_ratio;
      //printf("y=%d, x=%d, y2=%d, x2=%d, (y2*virtual_screen->w)+x2=%d\n", i, j, y2, x2, (y2*virtual_screen->w)+x2);
    }
  }
}

/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_Upscaling_Bilinear(uint16_t *src_screen, uint16_t *dst_screen,
								int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  uint32_t x_diff, y_diff;
  uint16_t red_comp, green_comp, blue_comp;
  uint32_t p_val_tl, p_val_tr, p_val_bl, p_val_br;
  int x, y ;
  //printf("virtual_screen->h=%d, h2=%d\n", virtual_screen->h, h2);

#ifdef BLACKER_BLACKS
      /// Optimization for blacker blacks (our screen do not handle green value of 1 very well)
      uint16_t green_mask = 0x07C0;
#else
      uint16_t green_mask = 0x07E0;
#endif

  /// --- Compute padding for centering when out of bounds ---
  int y_padding = (RES_HW_SCREEN_VERTICAL-dst_h)/2;
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    uint16_t* t = (uint16_t*)(dst_screen + (i+y_padding)*
					((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    y = ((i*y_ratio)>>16);
    y_diff = (i*y_ratio) - (y<<16) ;
    uint16_t* p = (uint16_t*)(src_screen + (y*w1 + x_padding_ratio) );
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      x = (rat>>16);
      x_diff = rat - (x<<16) ;
/*#ifdef BLACKER_BLACKS
      *t++ = p[x] & 0xFFDF; /// Optimization for blacker blacks
#else
      *t++ = p[x]; /// Optimization for blacker blacks
#endif*/
      /// --- Getting adjacent pixels ---
      p_val_tl = p[x] ;
      p_val_tr = (x+1<w1)?p[x+1]:p[x];
      p_val_bl = (y+1<h1)?p[x+w1]:p[x];
      p_val_br = (y+1<h1 && x+1<w1)?p[x+w1+1]:p[x];

      // red element
      // Yr = Ar(1-w)(1-h) + Br(w)(1-h) + Cr(h)(1-w) + Dr(wh)
      red_comp = (( ((p_val_tl&0xF800)>>11) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>5) )>>27) +
		(( ((p_val_tr&0xF800)>>11) * ((x_diff * ((1<<16)-y_diff))>>5) )>>27) +
            (( ((p_val_bl&0xF800)>>11) * ((y_diff * ((1<<16)-x_diff))>>5) )>>27) +
            (( ((p_val_br&0xF800)>>11) * ((y_diff * x_diff)>>5) )>>27);

      // green element
      // Yg = Ag(1-w)(1-h) + Bg(w)(1-h) + Cg(h)(1-w) + Dg(wh)
      green_comp = (( ((p_val_tl&0x07E0)>>5) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>6) )>>26) +
		(( ((p_val_tr&0x07E0)>>5) * ((x_diff * ((1<<16)-y_diff))>>6) )>>26) +
            (( ((p_val_bl&0x07E0)>>5) * ((y_diff * ((1<<16)-x_diff))>>6) )>>26) +
            (( ((p_val_br&0x07E0)>>5) * ((y_diff * x_diff)>>6) )>>26);

      // blue element
      // Yb = Ab(1-w)(1-h) + Bb(w)(1-h) + Cb(h)(1-w) + Db(wh)
      blue_comp = (( ((p_val_tl&0x001F)) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>5) )>>27) +
		(( ((p_val_tr&0x001F)) * ((x_diff * ((1<<16)-y_diff))>>5) )>>27) +
            (( ((p_val_bl&0x001F)) * ((y_diff * ((1<<16)-x_diff))>>5) )>>27) +
            (( ((p_val_br&0x001F)) * ((y_diff * x_diff)>>5) )>>27);

      /// --- Write pixel value ---
      *t++ = ((red_comp<<11)&0xF800) + ((green_comp<<5)&0x07E0) + ((blue_comp)&0x001F);

      /// --- Update x ----
      rat += x_ratio;
    }
  }
}




/// Interpolation with left, right pixels, pseudo gaussian weighting for downscaling
void flip_Downscale_LeftRightGaussianFilter(uint16_t *src_screen, uint16_t *dst_screen,
								int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  int y_padding = (RES_HW_SCREEN_VERTICAL-dst_h)/2;
  int x1, y1;

  /// --- Compute padding for centering when out of bounds ---
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  /// --- Interp params ---
  int px_diff_prev_x = 0;
  int px_diff_next_x = 0;
  uint32_t ponderation_factor;
  uint8_t left_px_missing, right_px_missing;

  uint16_t * cur_p;
  uint16_t * cur_p_left;
  uint16_t * cur_p_right;
  uint32_t red_comp, green_comp, blue_comp;


  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }
    uint16_t* t = (uint16_t*)(dst_screen +
	(i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    // ------ current and next y value ------
    y1 = ((i*y_ratio)>>16);
    uint16_t* p = (uint16_t*)(src_screen + (y1*w1+x_padding_ratio) );
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }

      // ------ current x value ------
      x1 = (rat>>16);
      px_diff_next_x = ((rat+x_ratio)>>16) - x1;

      // ------ adapted bilinear with 3x3 gaussian blur -------
      cur_p = p+x1;
      if(px_diff_prev_x > 1 || px_diff_next_x > 1 ){
        red_comp=((*cur_p)&0xF800) << 1;
        green_comp=((*cur_p)&0x07E0) << 1;
        blue_comp=((*cur_p)&0x001F) << 1;

        left_px_missing = (px_diff_prev_x > 1 && x1>0);
        right_px_missing = (px_diff_next_x > 1 && x1+1<w1);
        ponderation_factor = 2 + left_px_missing + right_px_missing;

        // ---- Interpolate current and left ----
        if(left_px_missing){
	    cur_p_left = p+x1-1;
            red_comp += ((*cur_p_left)&0xF800);
            green_comp += ((*cur_p_left)&0x07E0);
            blue_comp += ((*cur_p_left)&0x001F);
        }

        // ---- Interpolate current and right ----
        if(right_px_missing){
	    cur_p_right = p+x1+1;
            red_comp += ((*cur_p_right)&0xF800);
            green_comp += ((*cur_p_right)&0x07E0);
            blue_comp += ((*cur_p_right)&0x001F);
        }

        /// --- Compute new px value ---
        if(ponderation_factor==4){
		red_comp = (red_comp >> 2)&0xF800;
		green_comp = (green_comp >> 2)&0x07C0;
		blue_comp = (blue_comp >> 2)&0x001F;
        }
        else if(ponderation_factor==2){
		red_comp = (red_comp >> 1)&0xF800;
		green_comp = (green_comp >> 1)&0x07C0;
		blue_comp = (blue_comp >> 1)&0x001F;
        }
        else{
		red_comp = (red_comp / ponderation_factor )&0xF800;
		green_comp = (green_comp / ponderation_factor )&0x07C0;
		blue_comp = (blue_comp / ponderation_factor )&0x001F;
        }

        /// --- write pixel ---
        *t++ = red_comp+green_comp+blue_comp;
      }
      else{
        /// --- copy pixel ---
        *t++ = (*cur_p);

        /// Debug
        //occurence_pond[1] += 1;
      }

      /// save number of pixels to interpolate
      px_diff_prev_x = px_diff_next_x;

      // ------ next pixel ------
      rat += x_ratio;
    }
  }
}




/// Interpolation with left, right pixels, pseudo gaussian weighting for downscaling
void flip_Downscale_LeftRightUpDownGaussianFilter_Optimized4(uint16_t *src_screen, uint16_t *dst_screen,
								int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  int y_padding = (RES_HW_SCREEN_VERTICAL-dst_h)/2;
  int x1, y1;

  /// --- Compute padding for centering when out of bounds ---
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  /// --- Interp params ---
  int px_diff_prev_x = 0;
  int px_diff_next_x = 0;
  uint32_t ponderation_factor;
  uint8_t left_px_missing, right_px_missing;
  int supposed_pond_factor;

  uint16_t * cur_p;
  uint16_t * cur_p_left;
  uint16_t * cur_p_right;
  uint32_t red_comp, green_comp, blue_comp;
  //printf("virtual_screen->w=%d, virtual_screen->w=%d\n", virtual_screen->w, virtual_screen->h);

  ///Debug
  /*int occurence_pond[9];
  memset(occurence_pond, 0, 9*sizeof(int));*/

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }
    uint16_t* t = (uint16_t*)(dst_screen +
	(i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    // ------ current and next y value ------
    y1 = ((i*y_ratio)>>16);
    uint16_t* p = (uint16_t*)(src_screen + (y1*w1+x_padding_ratio) );
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      // ------ current x value ------
      x1 = (rat>>16);
      px_diff_next_x = ((rat+x_ratio)>>16) - x1;

      // ------ adapted bilinear with 3x3 gaussian blur -------
      cur_p = p+x1;
      if(px_diff_prev_x > 1 || px_diff_next_x > 1 ){
        red_comp=((*cur_p)&0xF800) << 1;
        green_comp=((*cur_p)&0x07E0) << 1;
        blue_comp=((*cur_p)&0x001F) << 1;
        ponderation_factor = 2;
        left_px_missing = (px_diff_prev_x > 1 && x1>0);
        right_px_missing = (px_diff_next_x > 1 && x1+1<w1);
        supposed_pond_factor = 2 + left_px_missing + right_px_missing;

        // ---- Interpolate current and left ----
        if(left_px_missing){
          cur_p_left = p+x1-1;

          if(supposed_pond_factor==3){
            red_comp += ((*cur_p_left)&0xF800) << 1;
            green_comp += ((*cur_p_left)&0x07E0) << 1;
            blue_comp += ((*cur_p_left)&0x001F) << 1;
            ponderation_factor+=2;
          }
          else if(supposed_pond_factor == 4){
            red_comp += ((*cur_p_left)&0xF800);
            green_comp += ((*cur_p_left)&0x07E0);
            blue_comp += ((*cur_p_left)&0x001F);
            ponderation_factor++;
          }
        }

        // ---- Interpolate current and right ----
        if(right_px_missing){
          cur_p_right = p+x1+1;

          if(supposed_pond_factor==3){
            red_comp += ((*cur_p_right)&0xF800) << 1;
            green_comp += ((*cur_p_right)&0x07E0) << 1;
            blue_comp += ((*cur_p_right)&0x001F) << 1;
            ponderation_factor+=2;
          }
          else if(supposed_pond_factor == 4){
            red_comp += ((*cur_p_right)&0xF800);
            green_comp += ((*cur_p_right)&0x07E0);
            blue_comp += ((*cur_p_right)&0x001F);
            ponderation_factor++;
          }
        }

        /// --- Compute new px value ---
        if(ponderation_factor==4){
          red_comp = (red_comp >> 2)&0xF800;
          green_comp = (green_comp >> 2)&0x07C0;
          blue_comp = (blue_comp >> 2)&0x001F;
        }
        else if(ponderation_factor==2){
          red_comp = (red_comp >> 1)&0xF800;
          green_comp = (green_comp >> 1)&0x07C0;
          blue_comp = (blue_comp >> 1)&0x001F;
        }
        else{
          red_comp = (red_comp / ponderation_factor )&0xF800;
          green_comp = (green_comp / ponderation_factor )&0x07C0;
          blue_comp = (blue_comp / ponderation_factor )&0x001F;
        }

        /// Debug
        //occurence_pond[ponderation_factor] += 1;

        /// --- write pixel ---
        *t++ = red_comp+green_comp+blue_comp;
      }
      else{
        /// --- copy pixel ---
        *t++ = (*cur_p);

        /// Debug
        //occurence_pond[1] += 1;
      }

      /// save number of pixels to interpolate
      px_diff_prev_x = px_diff_next_x;

      // ------ next pixel ------
      rat += x_ratio;
    }
  }
  /// Debug
  /*printf("pond: [%d, %d, %d, %d, %d, %d, %d, %d]\n", occurence_pond[1], occurence_pond[2], occurence_pond[3],
                                              occurence_pond[4], occurence_pond[5], occurence_pond[6],
                                              occurence_pond[7], occurence_pond[8]);*/
}



/// Interpolation with left, right pixels, pseudo gaussian weighting for downscaling
void flip_Downscale_LeftRightUpDownGaussianFilter_Optimized4Forward(uint16_t *src_screen, uint16_t *dst_screen,
								int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  int y_padding = (RES_HW_SCREEN_VERTICAL-dst_h)/2;
  int x1, y1;

  /// --- Compute padding for centering when out of bounds ---
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  /// --- Interp params ---
  int px_diff_next_x = 0;
  uint32_t ponderation_factor;
  uint8_t right_px_missing;
  int supposed_pond_factor;

  uint16_t * cur_p;
  uint16_t * cur_p_left;
  uint16_t * cur_p_right;
  uint32_t red_comp, green_comp, blue_comp;
  //printf("virtual_screen->w=%d, virtual_screen->w=%d\n", virtual_screen->w, virtual_screen->h);

  ///Debug
  /*int occurence_pond[9];
  memset(occurence_pond, 0, 9*sizeof(int));*/

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }
    uint16_t* t = (uint16_t*)(dst_screen +
	(i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    // ------ current and next y value ------
    y1 = ((i*y_ratio)>>16);
    uint16_t* p = (uint16_t*)(src_screen + (y1*w1+x_padding_ratio) );
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      // ------ current x value ------
      x1 = (rat>>16);
      px_diff_next_x = ((rat+x_ratio)>>16) - x1;

      // ------ adapted bilinear with 3x3 gaussian blur -------
      cur_p = p+x1;
      if(px_diff_next_x > 1 ){
        red_comp=((*cur_p)&0xF800) << 1;
        green_comp=((*cur_p)&0x07E0) << 1;
        blue_comp=((*cur_p)&0x001F) << 1;
        ponderation_factor = 2;
        right_px_missing = (px_diff_next_x > 1 && x1+1<w1);
        supposed_pond_factor = 2 + right_px_missing;

        // ---- Interpolate current and right ----
        if(right_px_missing){
		cur_p_right = p+x1+1;

		red_comp += ((*cur_p_right)&0xF800) << 1;
		green_comp += ((*cur_p_right)&0x07E0) << 1;
		blue_comp += ((*cur_p_right)&0x001F) << 1;
		ponderation_factor+=2;
        }

        /// --- Compute new px value ---
        if(ponderation_factor==4){
          red_comp = (red_comp >> 2)&0xF800;
          green_comp = (green_comp >> 2)&0x07C0;
          blue_comp = (blue_comp >> 2)&0x001F;
        }
        else if(ponderation_factor==2){
          red_comp = (red_comp >> 1)&0xF800;
          green_comp = (green_comp >> 1)&0x07C0;
          blue_comp = (blue_comp >> 1)&0x001F;
        }
        else{
          red_comp = (red_comp / ponderation_factor )&0xF800;
          green_comp = (green_comp / ponderation_factor )&0x07C0;
          blue_comp = (blue_comp / ponderation_factor )&0x001F;
        }

        /// Debug
        //occurence_pond[ponderation_factor] += 1;

        /// --- write pixel ---
        *t++ = red_comp+green_comp+blue_comp;
      }
      else{
        /// --- copy pixel ---
        *t++ = (*cur_p);

        /// Debug
        //occurence_pond[1] += 1;
      }

      // ------ next pixel ------
      rat += x_ratio;
    }
  }
  /// Debug
  /*printf("pond: [%d, %d, %d, %d, %d, %d, %d, %d]\n", occurence_pond[1], occurence_pond[2], occurence_pond[3],
                                              occurence_pond[4], occurence_pond[5], occurence_pond[6],
                                              occurence_pond[7], occurence_pond[8]);*/
}


/// Interpolation with left, right, up and down pixels, pseudo gaussian weighting for downscaling
void flip_Downscale_LeftRightUpDownGaussianFilter_Optimized8(uint16_t *src_screen, uint16_t *dst_screen,
								int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  int y_padding = (RES_HW_SCREEN_VERTICAL-dst_h)/2;
  int x1, y1;

#ifdef BLACKER_BLACKS
      /// Optimization for blacker blacks (our screen do not handle green value of 1 very well)
      uint16_t green_mask = 0x07C0;
#else
      uint16_t green_mask = 0x07E0;
#endif

  /// --- Compute padding for centering when out of bounds ---
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  /// --- Interp params ---
  int px_diff_prev_x = 0;
  int px_diff_next_x = 0;
  int px_diff_prev_y = 0;
  int px_diff_next_y = 0;
  uint32_t ponderation_factor;
  uint8_t left_px_missing, right_px_missing, up_px_missing, down_px_missing;
  int supposed_pond_factor;

  uint16_t * cur_p;
  uint16_t * cur_p_left;
  uint16_t * cur_p_right;
  uint16_t * cur_p_up;
  uint16_t * cur_p_down;
  uint32_t red_comp, green_comp, blue_comp;
  //printf("virtual_screen->w=%d, virtual_screen->w=%d\n", virtual_screen->w, virtual_screen->h);

  ///Debug
  /*int occurence_pond[9];
  memset(occurence_pond, 0, 9*sizeof(int));*/

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }
    uint16_t* t = (uint16_t*)(dst_screen +
	(i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    // ------ current and next y value ------
    y1 = ((i*y_ratio)>>16);
    px_diff_next_y = MAX( (((i+1)*y_ratio)>>16) - y1, 1);
    uint16_t* p = (uint16_t*)(src_screen + (y1*w1+x_padding_ratio) );
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      // ------ current x value ------
      x1 = (rat>>16);
      px_diff_next_x = ((rat+x_ratio)>>16) - x1;

      // ------ adapted bilinear with 3x3 gaussian blur -------
      cur_p = p+x1;
      if(px_diff_prev_x > 1 || px_diff_next_x > 1 || px_diff_prev_y > 1 || px_diff_next_y > 1){
        red_comp=((*cur_p)&0xF800) << 1;
        green_comp=((*cur_p)&0x07E0) << 1;
        blue_comp=((*cur_p)&0x001F) << 1;
        ponderation_factor = 2;
        left_px_missing = (px_diff_prev_x > 1 && x1>0);
        right_px_missing = (px_diff_next_x > 1 && x1+1<w1);
        up_px_missing = (px_diff_prev_y > 1 && y1 > 0);
        down_px_missing = (px_diff_next_y > 1 && y1 + 1 < h1);
        supposed_pond_factor = 2 + left_px_missing + right_px_missing +
                                       up_px_missing + down_px_missing;

        // ---- Interpolate current and up ----
        if(up_px_missing){
          cur_p_up = p+x1-w1;

          if(supposed_pond_factor==3){
            red_comp += ((*cur_p_up)&0xF800) << 1;
            green_comp += ((*cur_p_up)&0x07E0) << 1;
            blue_comp += ((*cur_p_up)&0x001F) << 1;
            ponderation_factor+=2;
          }
          else if(supposed_pond_factor == 4 ||
                  (supposed_pond_factor == 5 && !down_px_missing) ||
                  supposed_pond_factor == 6 ){
            red_comp += ((*cur_p_up)&0xF800);
            green_comp += ((*cur_p_up)&0x07E0);
            blue_comp += ((*cur_p_up)&0x001F);
            ponderation_factor++;
          }
        }

        // ---- Interpolate current and left ----
        if(left_px_missing){
          cur_p_left = p+x1-1;

          if(supposed_pond_factor==3){
            red_comp += ((*cur_p_left)&0xF800) << 1;
            green_comp += ((*cur_p_left)&0x07E0) << 1;
            blue_comp += ((*cur_p_left)&0x001F) << 1;
            ponderation_factor+=2;
          }
          else if(supposed_pond_factor == 4 ||
                  (supposed_pond_factor == 5 && !right_px_missing) ||
                  supposed_pond_factor == 6 ){
            red_comp += ((*cur_p_left)&0xF800);
            green_comp += ((*cur_p_left)&0x07E0);
            blue_comp += ((*cur_p_left)&0x001F);
            ponderation_factor++;
          }
        }

        // ---- Interpolate current and down ----
        if(down_px_missing){
          cur_p_down = p+x1+w1;

          if(supposed_pond_factor==3 || supposed_pond_factor==6){
            red_comp += ((*cur_p_down)&0xF800) << 1;
            green_comp += ((*cur_p_down)&0x07E0) << 1;
            blue_comp += ((*cur_p_down)&0x001F) << 1;
            ponderation_factor+=2;
          }
          else if(supposed_pond_factor >= 4 && supposed_pond_factor != 6){
            red_comp += ((*cur_p_down)&0xF800);
            green_comp += ((*cur_p_down)&0x07E0);
            blue_comp += ((*cur_p_down)&0x001F);
            ponderation_factor++;
          }
        }

        // ---- Interpolate current and right ----
        if(right_px_missing){
          cur_p_right = p+x1+1;

          if(supposed_pond_factor==3 || supposed_pond_factor==6){
            red_comp += ((*cur_p_right)&0xF800) << 1;
            green_comp += ((*cur_p_right)&0x07E0) << 1;
            blue_comp += ((*cur_p_right)&0x001F) << 1;
            ponderation_factor+=2;
          }
          else if(supposed_pond_factor >= 4 && supposed_pond_factor != 6){
            red_comp += ((*cur_p_right)&0xF800);
            green_comp += ((*cur_p_right)&0x07E0);
            blue_comp += ((*cur_p_right)&0x001F);
            ponderation_factor++;
          }
        }

        /// --- Compute new px value ---
        if(ponderation_factor==8){
          red_comp = (red_comp >> 3)&0xF800;
          green_comp = (green_comp >> 3)&green_mask;
          blue_comp = (blue_comp >> 3)&0x001F;
        }
        else if(ponderation_factor==4){
          red_comp = (red_comp >> 2)&0xF800;
          green_comp = (green_comp >> 2)&green_mask;
          blue_comp = (blue_comp >> 2)&0x001F;
        }
        else if(ponderation_factor==2){
          red_comp = (red_comp >> 1)&0xF800;
          green_comp = (green_comp >> 1)&green_mask;
          blue_comp = (blue_comp >> 1)&0x001F;
        }
        else{
          red_comp = (red_comp / ponderation_factor )&0xF800;
          green_comp = (green_comp / ponderation_factor )&green_mask;
          blue_comp = (blue_comp / ponderation_factor )&0x001F;
        }

        /// Debug
        //occurence_pond[ponderation_factor] += 1;

        /// --- write pixel ---
        *t++ = red_comp+green_comp+blue_comp;
      }
      else{
        /// --- copy pixel ---
#ifdef BLACKER_BLACKS
        /// Optimization for blacker blacks (our screen do not handle green value of 1 very well)
        *t++ = (*cur_p)&0xFFDF;
#else
        *t++ = (*cur_p);
#endif

        /// Debug
        //occurence_pond[1] += 1;
      }

      /// save number of pixels to interpolate
      px_diff_prev_x = px_diff_next_x;

      // ------ next pixel ------
      rat += x_ratio;
    }
    px_diff_prev_y = px_diff_next_y;
  }
  /// Debug
  /*printf("pond: [%d, %d, %d, %d, %d, %d, %d, %d]\n", occurence_pond[1], occurence_pond[2], occurence_pond[3],
                                              occurence_pond[4], occurence_pond[5], occurence_pond[6],
                                              occurence_pond[7], occurence_pond[8]);*/
}

/*
void SDL_Copy_Rotate_270(uint16_t *source_pixels, uint16_t *dest_pixels,
								int src_w, int src_h, int dst_w, int dst_h){
	int i, j;

    /// --- Checking if same dimensions ---
    if(dst_w != src_w || dst_h != src_h){
	printf("Error in SDL_Rotate_270, dest_pixels (%dx%d) and source_pixels (%dx%d) have different dimensions\n",
		dst_w, dst_h, src_w, src_h);
	return;
    }

	/// --- Pixel copy and rotation (270) ---
	uint16_t *cur_p_src, *cur_p_dst;
	for(i=0; i<src_h; i++){
		for(j=0; j<src_w; j++){
			cur_p_src = source_pixels + i*src_w + j;
			cur_p_dst = dest_pixels + (dst_h-1-j)*dst_w + i;
			*cur_p_dst = *cur_p_src;
		}
	}
}*/





/// Interpolation with left, right pixels, pseudo gaussian weighting for downscaling - operations on 16bits
void flip_Downscale_LeftRightGaussianFilter_Optimized(uint16_t *src_screen, uint16_t *dst_screen, int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;
  //printf("src = %dx%d\n", w1, h1);
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  int y_padding = (RES_HW_SCREEN_VERTICAL-h2)/2;
  int x1, y1;

  /// --- Compute padding for centering when out of bounds ---
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  /// --- Interp params ---
  int px_diff_prev_x = 0;
  int px_diff_next_x = 0;
  uint8_t left_px_missing, right_px_missing;

  uint16_t * cur_p;
  uint16_t * cur_p_left;
  uint16_t * cur_p_right;


  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }
    uint16_t* t = (uint16_t*)(dst_screen +
      (i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    // ------ current and next y value ------
    y1 = ((i*y_ratio)>>16);
    uint16_t* p = (uint16_t*)(src_screen + (y1*w1+x_padding_ratio) );
    int rat = 0;

    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }

      // ------ current x value ------
      x1 = (rat>>16);
      px_diff_next_x = ((rat+x_ratio)>>16) - x1;

      //printf("x1=%d, px_diff_prev_x=%d, px_diff_next_x=%d\n", x1, px_diff_prev_x, px_diff_next_x);

      // ------ adapted bilinear with 3x3 gaussian blur -------
      cur_p = p+x1;
      if(px_diff_prev_x > 1 || px_diff_next_x > 1 ){

        left_px_missing = (px_diff_prev_x > 1 && x1>0);
        right_px_missing = (px_diff_next_x > 1 && x1+1<w1);
        cur_p_left = cur_p-1;
        cur_p_right = cur_p+1;

        // ---- Interpolate current and left ----
        if(left_px_missing && !right_px_missing){
          *t++ = Weight1_1(*cur_p, *cur_p_left);
          //*t++ = Weight1_1(*cur_p, Weight1_3(*cur_p, *cur_p_left));
        }
        // ---- Interpolate current and right ----
        else if(right_px_missing && !left_px_missing){
          *t++ = Weight1_1(*cur_p, *cur_p_right);
          //*t++ = Weight1_1(*cur_p, Weight1_3(*cur_p, *cur_p_right));
        }
        // ---- Interpolate with Left and right pixels
        else{
          *t++ = Weight1_1(Weight1_1(*cur_p, *cur_p_left), Weight1_1(*cur_p, *cur_p_right));
        }

      }
      else{
        /// --- copy pixel ---
        *t++ = (*cur_p);

        /// Debug
        //occurence_pond[1] += 1;
      }

      /// save number of pixels to interpolate
      px_diff_prev_x = px_diff_next_x;

      // ------ next pixel ------
      rat += x_ratio;
    }
  }
}

/// Interpolation with left, right pixels, pseudo gaussian weighting for downscaling - operations on 16bits
void flip_Downscale_LeftRightGaussianFilter_Optimized_mergeUpDown(uint16_t *src_screen, uint16_t *dst_screen, int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;
  //printf("src = %dx%d\n", w1, h1);
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  int y_padding = (RES_HW_SCREEN_VERTICAL-h2)/2;
  int x1;

  /// --- Compute padding for centering when out of bounds ---
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  /// --- Interp params ---
  int px_diff_prev_x = 0;
  int px_diff_next_x = 0;
  uint8_t left_px_missing, right_px_missing;

  uint16_t * cur_p;
  uint16_t * cur_p_left;
  uint16_t * cur_p_right;

  int y1=0, prev_y1=-1, prev_prev_y1=-2;
  uint16_t *prev_t, *t_init=dst_screen;

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }
    prev_t = t_init;
    t_init = (uint16_t*)(dst_screen +
      (i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    uint16_t *t = t_init;

    // ------ current and next y value ------
    prev_prev_y1 = prev_y1;
    prev_y1 = y1;
    y1 = ((i*y_ratio)>>16);

    uint16_t* p = (uint16_t*)(src_screen + (y1*w1+x_padding_ratio) );
    int rat = 0;

    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }

      // ------ current x value ------
      x1 = (rat>>16);
      px_diff_next_x = ((rat+x_ratio)>>16) - x1;

      //printf("x1=%d, px_diff_prev_x=%d, px_diff_next_x=%d\n", x1, px_diff_prev_x, px_diff_next_x);

      // ------ adapted bilinear with 3x3 gaussian blur -------
      cur_p = p+x1;
      if(px_diff_prev_x > 1 || px_diff_next_x > 1 ){

        left_px_missing = (px_diff_prev_x > 1 && x1>0);
        right_px_missing = (px_diff_next_x > 1 && x1+1<w1);
        cur_p_left = cur_p-1;
        cur_p_right = cur_p+1;

        // ---- Interpolate current and left ----
        if(left_px_missing && !right_px_missing){
          *t++ = Weight1_1(*cur_p, *cur_p_left);
          //*t++ = Weight1_1(*cur_p, Weight1_3(*cur_p, *cur_p_left));
        }
        // ---- Interpolate current and right ----
        else if(right_px_missing && !left_px_missing){
          *t++ = Weight1_1(*cur_p, *cur_p_right);
          //*t++ = Weight1_1(*cur_p, Weight1_3(*cur_p, *cur_p_right));
        }
        // ---- Interpolate with Left and right pixels
        else{
          *t++ = Weight1_1(Weight1_1(*cur_p, *cur_p_left), Weight1_1(*cur_p, *cur_p_right));
        }

      }
      else{
        /// --- copy pixel ---
        *t++ = (*cur_p);

        /// Debug
        //occurence_pond[1] += 1;
      }

      // y_merge
      if(prev_y1 == prev_prev_y1 && y1 != prev_y1){
        //printf("we are here %d\n", ++count);
        *(prev_t    ) = Weight1_1(*(t    ), *(prev_t    ));
      }

      /// save number of pixels to interpolate
      px_diff_prev_x = px_diff_next_x;

      // ------ next pixel ------
      rat += x_ratio;
    }
  }
}


/// Interpolation with left, right pixels, pseudo gaussian weighting for downscaling - operations on 16bits
void flip_optimizedWidth_256to240(uint16_t *src_screen, uint16_t *dst_screen, int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;

  if(w1!=256){
    printf("src_w (%d) != 256\n", src_w);
    return;
  }

  //printf("src = %dx%d\n", w1, h1);
  int y_ratio = (int)((h1<<16)/h2);
  int y_padding = (RES_HW_SCREEN_VERTICAL-h2)/2;

  /* Interpolation */
  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    uint16_t *t = (uint16_t*)(dst_screen +
      (i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );

    // ------ current and next y value ------
    int y1 = ((i*y_ratio)>>16);

    uint16_t* p = (uint16_t*)(src_screen + (y1*w1) );

    for (int j=0;j<16;j++)
    {
      /* Horizontaly:
       * Before(16):
       * (a)(b)(c)(d)(e)(f)(g)(h)(i)(j)(k)(l)(m)(n)(o)(p)
       * After(15):
       * (a)(b)(c)(d)(e)(f)(g)(h)(i)(j)(k)(l)(mmmn)(no)(oppp)
       */
	/*
      *(t     ) = *(p    );
      *(t +  1) = *(p + 1);
      *(t +  2) = *(p + 2);
      *(t +  3) = *(p + 3);
      *(t +  4) = *(p + 4);
      *(t +  5) = *(p + 5);
      *(t +  6) = *(p + 6);
      *(t +  7) = *(p + 7);
      *(t +  8) = *(p + 8);
      *(t +  9) = *(p + 9);
      *(t + 10) = *(p + 10);
      *(t + 11) = *(p + 11);
      *(t + 12) = Weight3_1( *(p + 12), *(p + 13) );
      *(t + 13) = Weight1_1( *(p + 13), *(p + 14) );
      *(t + 14) = Weight1_3( *(p + 14), *(p + 15) );
      */

      *(t     ) = *(p    );
      *(t +  1) = *(p + 1);
      *(t +  2) = *(p + 2);
      *(t +  3) = *(p + 3);
      *(t +  4) = *(p + 4);
      *(t +  5) = *(p + 5);
      *(t +  6) = *(p + 6);
      *(t +  7) = *(p + 7);
      *(t +  8) = *(p + 8);
      *(t +  9) = *(p + 9);
      *(t + 10) = *(p + 10);
      *(t + 11) = *(p + 11);
      *(t + 12) = Weight3_1( *(p + 12), *(p + 13) );
      *(t + 13) = Weight1_1( *(p + 13), *(p + 14) );
      *(t + 14) = Weight1_3( *(p + 14), *(p + 15) );

      // ------ next dst pixel ------
      t+=15;
      p+=16;
    }
  }
}


/// Interpolation with left, right pixels, pseudo gaussian weighting for downscaling - operations on 16bits
void flip_optimizedWidth_256to240_mergeUpDown(uint16_t *src_screen, uint16_t *dst_screen, int src_w, int src_h, int dst_w, int dst_h){
  int w1=src_w;
  int h1=src_h;
  int w2=dst_w;
  int h2=dst_h;

  if(w1!=256){
    printf("src_w (%d) != 256\n", src_w);
    return;
  }

  //printf("src = %dx%d\n", w1, h1);
  int y_ratio = (int)((h1<<16)/h2);
  int y_padding = (RES_HW_SCREEN_VERTICAL-h2)/2;
  int y1=0, prev_y1=-1, prev_prev_y1=-2;

  uint16_t *prev_t, *t_init=dst_screen;

  /* Interpolation */
  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    prev_t = t_init;
    t_init = (uint16_t*)(dst_screen +
      (i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) );
    uint16_t *t = t_init;

    // ------ current and next y value ------
    prev_prev_y1 = prev_y1;
    prev_y1 = y1;
    y1 = ((i*y_ratio)>>16);

    uint16_t* p = (uint16_t*)(src_screen + (y1*w1) );

    for (int j=0;j<16;j++)
    {
      /* Horizontaly:
       * Before(16):
       * (a)(b)(c)(d)(e)(f)(g)(h)(i)(j)(k)(l)(m)(n)(o)(p)
       * After(15):
       * (a)(b)(c)(d)(e)(f)(g)(h)(i)(j)(k)(l)(mmmn)(no)(oppp)
       */
      *(t     ) = *(p    );
      *(t +  1) = *(p + 1);
      *(t +  2) = *(p + 2);
      *(t +  3) = *(p + 3);
      *(t +  4) = *(p + 4);
      *(t +  5) = *(p + 5);
      *(t +  6) = *(p + 6);
      *(t +  7) = *(p + 7);
      *(t +  8) = *(p + 8);
      *(t +  9) = *(p + 9);
      *(t + 10) = *(p + 10);
      *(t + 11) = *(p + 11);
      *(t + 12) = Weight3_1( *(p + 12), *(p + 13) );
      *(t + 13) = Weight1_1( *(p + 13), *(p + 14) );
      *(t + 14) = Weight1_3( *(p + 14), *(p + 15) );

      if(prev_y1 == prev_prev_y1 && y1 != prev_y1){
	/*static int count = 0;
        printf("we are here %d\n", ++count);*/
        *(prev_t    ) = Weight1_1(*(t    ), *(prev_t    ));
        *(prev_t + 1) = Weight1_1(*(t + 1), *(prev_t + 1));
        *(prev_t + 2) = Weight1_1(*(t + 2), *(prev_t + 2));
        *(prev_t + 3) = Weight1_1(*(t + 3), *(prev_t + 3));
        *(prev_t + 4) = Weight1_1(*(t + 4), *(prev_t + 4));
        *(prev_t + 5) = Weight1_1(*(t + 5), *(prev_t + 5));
        *(prev_t + 6) = Weight1_1(*(t + 6), *(prev_t + 6));
        *(prev_t + 7) = Weight1_1(*(t + 7), *(prev_t + 7));
        *(prev_t + 8) = Weight1_1(*(t + 8), *(prev_t + 8));
        *(prev_t + 9) = Weight1_1(*(t + 9), *(prev_t + 9));
        *(prev_t + 10) = Weight1_1(*(t + 10), *(prev_t + 10));
        *(prev_t + 11) = Weight1_1(*(t + 11), *(prev_t + 11));
        *(prev_t + 12) = Weight1_1(*(t + 12), *(prev_t + 12));
        *(prev_t + 13) = Weight1_1(*(t + 13), *(prev_t + 13));
        *(prev_t + 14) = Weight1_1(*(t + 14), *(prev_t + 14));
      }


      // ------ next dst pixel ------
      t+=15;
      prev_t+=15;
      p+=16;
    }
  }
}



void clear_screen(uint16_t *screen_pixels, int w, int h, uint16_t color)
{
    uint32_t x, y;
    for(y = 0; y < h; y++)
    {
      for(x = 0; x < w; x++, screen_pixels++)
      {
        *screen_pixels = color;
      }
    }
}
