// DOKI DOKI LITERATURE CLUB //
// PS3 PORT // 
// SUPAHAXOR // 07/07/2025 //
// GAME IS COPYRIGHT TO TEAM SALVATO //
// V4.20 // SOURCE.c //
// DOING YANDERE DEV PROUD //

#include <ppu-lv2.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include <io/pad.h>
#include "rsxutil.h"

#include <audio/audio.h>
//#include <cell/fs/cell_fs_file_api.h>
#include <sysmodule/sysmodule.h>

#define MAX_BUFFERS 2
#define IMAGE_WIDTH 1280  // DEFAULT VALUES
#define IMAGE_HEIGHT 720  // DEFAULT VALUES

#define BG_PATH "/dev_hdd0/images/bg/"
#define MONIKA_PATH "/dev_hdd0/images/monika/"
#define SAYORI_PATH "/dev_hdd0/images/sayori/"

gcmContextData *context;

gcmTexture backgroundTexture;
gcmTexture monikaTexture;

rsxVertexProgram vertexProgram;
rsxFragmentProgram fragmentProgram;

gcmSurface* testSurface;

u32 *texture_buffer;
u32 texture_offset;

int currentBuffer; // Current buffer index for double buffering
rsxBuffer buffers[MAX_BUFFERS]; // Array of buffers for double buffering
void* shader;
void* fragShader;


uint8_t *rawTexture = NULL;
void *host_addr = NULL;


char path_buffer[256];    // Stores the path to the image file

  typedef struct {
  float x, y, z, w;
  float u, v; // Texture coordinates
  uint32_t colour;
} Vertex;

void* load_image_to_RSX(const char* path, rsxBuffer* buffer, int width, int height){ // This part works

  FILE* file = fopen(path, "rb");
  if (!file) {
    printf("Failed to open image: %s\n", path);
    return NULL;
  }
  size_t imageSize = width * height * 4; // Assuming ARGB format (4 bytes per pixel)
  printf("Requesting RSX memory for texture: %s (%d bytes)\n", path, (int)imageSize);

  void* rsx_texture_mem = rsxMemalign(128, imageSize);

  if (!rsx_texture_mem) {
    printf("Failed to allocate memory for texture: %s\n", path);
    fclose(file);
    return NULL;
  } 
  
  printf("Allocated memory for texture: %s\n", path);
  
  void* temp_buffer = malloc(imageSize);
  if(!temp_buffer) {
      printf("Failed to allocate temporary buffer for image: %s\n", path);
      rsxFree(rsx_texture_mem);
      fclose(file);
      return NULL;
  }
  size_t readBytes = fread(temp_buffer, 1, imageSize, file);
  if (readBytes != imageSize) {
    printf("Failed to read image data: %s (read %zu bytes)\n", path, readBytes);
    free(temp_buffer);
    return NULL;
  }
    
    memcpy(rsx_texture_mem, temp_buffer, imageSize);
    free(temp_buffer);
    fclose(file);

    printf("Image loaded successfully (%d bytes): %s\n", (int)imageSize, path);
    return rsx_texture_mem;
  
}

void drawFrame(rsxBuffer *buffer, long frame) {
  s32 i, j;
  for(i = 0; i < buffer->height; i++) {
    s32 color = (i / (buffer->height * 1.0) * 256);
    // This should make a nice black to green gradient
    color = (color << 8) | ((frame % 255) << 16);
    for(j = 0; j < buffer->width; j++)
      buffer->ptr[i* buffer->width + j] = color;
  }
}

void drawImage(rsxBuffer *buffer, const u8 *image_data, int image_width, int image_height){
  for (int y = 0; y < buffer->height && y < image_height; y++) {
    for (int x = 0; x < buffer->width && x < image_width; x++) {
      int i = y * image_width + x;
      int a = image_data[i * 4 + 0];
      int r = image_data[i * 4 + 1];
      int g = image_data[i * 4 + 2];
      int b = image_data[i * 4 + 3];

      // Pack ARGB to match RSX format
      buffer->ptr[y * buffer->width + x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    // Draws image data to the CPU.
    
  }
}

gcmTexture createTexture(gcmTexture* texture, void* imageData, int width, int height){
  texture->format = GCM_TEXTURE_FORMAT_A8R8G8B8;
  texture->width = width;
  texture->height = height;
  texture->dimension = GCM_TEXTURE_DIMS_2D;
  texture->mipmap = 0; // No mipmaps for this texture
  texture->pitch = width * 4; // Assuming ARGB format (4 bytes per pixel)
  texture->offset = texture_offset; // Offset in RSX memory, can be set later
  texture->cubemap = GCM_FALSE;

  texture->remap		= ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
						          (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
						          (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
						          (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
						          (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
						          (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
						          (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
						          (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));

  texture->location = GCM_LOCATION_RSX;
  //texture->swizzleMode = GCM_TEXTURE_SWIZZLE_MODE_NONE;
  rsxAddressToOffset(imageData, &texture_offset);
  
  printf("Creating texture: %dx%d\n", width, height);
  if (rsxAddressToOffset(imageData, &texture_offset)) {
    printf("Failed to convert address to offset for texture: %dx%d\n", width, height);
    //return texture;
  }

  return *texture; // Return the created texture

  //rsxLoadTexture(context, 0, texture);
}

gcmSurface createSurface(gcmSurface* surface, rsxBuffer* buffer) {

  surface->width = buffer->width;
  surface->height = buffer->height;
  surface->colorFormat =  GCM_SURFACE_X8R8G8B8;
  surface->colorLocation[0] = GCM_LOCATION_RSX;
  surface->depthPitch = buffer->width * 2; 
  surface->colorPitch[0] = buffer->width * 4; // Assuming ARGB format (4 bytes per pixel)
  surface->colorOffset[0] = buffer->offset; // Offset in RSX memory
  surface->depthLocation = GCM_LOCATION_RSX;

  surface->depthFormat = GCM_SURFACE_ZETA_Z16; // Using Z16 for depth format
  surface->depthOffset = buffer->offset;

  surface->type = 0x00;            // or SWIZZLED
  surface->antiAlias = 0x00; // No anti-aliasing
  surface->width = buffer->width;
  surface->height = buffer->height;
  surface->x = 0;
  surface->y = 0;
  return *surface;
}

void* loadShader(const char* shaderFile){
  FILE* file = fopen(shaderFile, "rb");
  if (!file) {
    printf("Failed to open vertex program file\n");
    return NULL;
  }
  
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  void* shaderData = rsxMemalign(size,16);
  if (!shaderData) {
    printf("Failed to allocate memory for vertex program\n");
    fclose(file);
    return NULL;
  }

  printf("File size reported: %lu bytes\n", (unsigned long)size);
       
  size_t bytesRead = fread(shaderData, 1, size, file);
  printf("Bytes read from file: %lu\n", (unsigned long)bytesRead);

  if (bytesRead != size) {
    printf("Warning: bytesRead != size!\n");
  }
  fclose(file);

  printf("Shader data starts with: %02X %02X %02X %02X\n", 
     ((uint8_t*)shaderData)[0], 
     ((uint8_t*)shaderData)[1],
     ((uint8_t*)shaderData)[2],
     ((uint8_t*)shaderData)[3]);

  if (bytesRead != size) {
    printf("Failed to read entire shader file\n");
    //rsxMemfree(shaderData);
    return NULL;
  }
  return shaderData; // Return the shader data to be used later.
}

void createQuad(){


  void* vertexBufferRSX = rsxMemalign(128, sizeof(Vertex) * 6);
  if (!vertexBufferRSX) {
    printf("Failed to allocate memory for vertex buffer\n");
    return;
  }

  Vertex quadVertices[6] = {
  { -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-left
  {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f }, // Bottom-right
  { -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0.0f }, // Top-left

  { -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0.0f }, // Top-left
  {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f }, // Bottom-right
  {  1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f }  // Top-right
  };
  memcpy(vertexBufferRSX, quadVertices, sizeof(Vertex) * 6);

  u32 offset;
  rsxAddressToOffset(vertexBufferRSX, &offset);

  
  rsxBindVertexArrayAttrib(context, GCM_VERTEX_ATTRIB_POS, 0, offset, sizeof(Vertex), 3,GCM_VERTEX_DATA_TYPE_F32,GCM_LOCATION_RSX); // Position
  rsxBindVertexArrayAttrib(context, GCM_VERTEX_ATTRIB_TEX0, 1, (void*)(offset + offsetof(Vertex, u)), sizeof(Vertex), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
  // Can't just push it as a string, it needs to be actually loaded into memory.
  // We need to write a vertex program to handle the vertex data... Whatever the hell that is.
  
  rsxDrawVertexArray(context, GCM_TYPE_TRIANGLES, 0, 6);
  rsxSetSurface(context, &buffers[currentBuffer]); // Set the surface to draw on

  //rsxBindTexture(context, 0, &backgroundTexture); // Bind the texture to the RSX context

}

void init_texture(u8 data)
{
	u32 i;
	u8 *buffer;
	

	texture_buffer = (u32*)rsxMemalign(128,(IMAGE_WIDTH*IMAGE_HEIGHT*4));
	if(!texture_buffer) return;

	rsxAddressToOffset(texture_buffer,&texture_offset);

	buffer = (u8*)texture_buffer;
	for(i=0;i<IMAGE_WIDTH*IMAGE_HEIGHT*4;i+=4) {
		buffer[i + 1] = data++;
		buffer[i + 2] = data++;
		buffer[i + 3] = data++;
		buffer[i + 0] = data++;
	}
}

int main(s32 argc, const char* argv[])
{
  //sysModuleLoad(SYSMODULE_FS);
  //sysModuleLoad(SYSMODULE_AUDIO);
  //cellAudioInit();
  
  buffers[MAX_BUFFERS];
  currentBuffer = 0;
  padInfo padinfo;
  padData paddata;
  u16 width = 1280;
  u16 height = 720;
  int i;

  // Assuming you have a buffers array with your framebuffers
// and context is your RSX context pointer

  host_addr = memalign (1024*1024, HOST_SIZE);
  context = initScreen (host_addr, HOST_SIZE);

  void* imageData = load_image_to_RSX(BG_PATH "bedroom.raw", &buffers[0], IMAGE_WIDTH, IMAGE_HEIGHT);
  void* monikaData = load_image_to_RSX(MONIKA_PATH "a.raw", &buffers[0], 500, 500);
  shader = loadShader("/dev_hdd0/V4.20/simple.vp");
  rsxLoadVertexProgram(context, &vertexProgram, shader);
  //fragShader = loadShader("/dev_hdd0/V4.20/fragShader.fp");
  gcmTexture newTexture = createTexture(&backgroundTexture, imageData, IMAGE_WIDTH, IMAGE_HEIGHT); // Need to return imageData and pass it into here.
  gcmSurface newSurface = createSurface(testSurface, &buffers[currentBuffer]);//createTexture(&monikaTexture, monikaData, 500, 500); // Create the texture for Monika's image.
  
  
  //rsxLoadFragmentProgram(context, &fragmentProgram, fragShader);
   // Draw the image data to the RSX buffer.
  rsxLoadTexture(context, 1, &newTexture);
  
  ioPadInit(7);

  getResolution(&width, &height);
  for (i = 0; i < MAX_BUFFERS; i++)
    makeBuffer( &buffers[i], width, height, i);

  flip(context, MAX_BUFFERS - 1);
  
  long frame = 0; // To keep track of how many frames we have rendered.
	
  // Ok, everything is setup. Now for the main loop.
  while(1){
    // Check the pads.
    /*ioPadGetInfo(&padinfo);
    for(i=0; i<MAX_PADS; i++){
      if(padinfo.status[i]){
	ioPadGetData(i, &paddata);*/
			
    
    //drawImage(&buffers[0], imageData, IMAGE_WIDTH, IMAGE_HEIGHT); // Loads a single image into the first buffer.
    
    //drawImage(&buffers[1], imageData, IMAGE_WIDTH, IMAGE_HEIGHT); // Loads the same image into the second buffer.
    

	/*if(paddata.BTN_START){
	  goto end;
	}
      }
			
    } */

    waitFlip(); // Wait for the last flip to finish, so we can draw to the old buffer
    //drawFrame(&buffers[currentBuffer], frame++); // Draw into the unused buffer
    //drawImage(&buffers[currentBuffer], imageData, IMAGE_WIDTH, IMAGE_HEIGHT);
    //load_raw_argb(&buffers[currentBuffer], "bedroom.raw", BG_PATH, IMAGE_WIDTH, IMAGE_HEIGHT);
    //load_and_draw_bg(&buffers[currentBuffer], "2r.raw", MONIKA_PATH, IMAGE_WIDTH, IMAGE_HEIGHT);
    // This is not SDL - you can't layer one image over another and call it a day.
    
    flip(context, buffers[currentBuffer].id); // Flip buffer onto screen

    currentBuffer++;
    if (currentBuffer >= MAX_BUFFERS)
      currentBuffer = 0;
  }
  
 end:

  gcmSetWaitFlip(context);
  for (i = 0; i < MAX_BUFFERS; i++)
    rsxFree(buffers[i].ptr);

  rsxFinish(context, 1);
  free(host_addr);
  ioPadEnd();
	
  return 0;
}
