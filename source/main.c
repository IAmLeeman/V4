// DOKI DOKI LITERATURE CLUB //
// PS3 PORT // 
// SUPAHAXOR // 30/06/2025 //
// GAME IS COPYRIGHT TO TEAM SALVATO //
// V4.20 // SOURCE.c //

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
rsxVertexProgram vertexProgram;
rsxFragmentProgram fragmentProgram;

gcmSurface testSurface;

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

void load_raw_argb(rsxBuffer* buffer, const char* path, const char* batch, int width, int height) {
  // Only for testing purposes, this function loads a raw ARGB image file

  snprintf(path_buffer, sizeof(path_buffer), "%s%s", batch, path);

  FILE* file = fopen(path_buffer, "rb");
  if (!file) {
    printf("Failed to open image: %s\n", path);
    return;
  }
  
  size_t imageSize = width * height * 4; // Assuming ARGB format (4 bytes per pixel)
  u8* imageData = (u8*)malloc(imageSize);
  if (!imageData) {
    printf("Failed to allocate memory for image data: %s\n", path);
    fclose(file);
    return;
  }
  // Draw the image data to the RSX buffer
  drawImage(buffer, imageData, width, height);
  
  // Free the allocated memory for the image data
  //rsxFree(imageData);
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

void createTexture(gcmTexture* texture, void* imageData, int width, int height){
  texture->format = GCM_TEXTURE_FORMAT_A8R8G8B8;
  texture->width = width;
  texture->height = height;
  texture->dimension = GCM_TEXTURE_DIMS_2D;
  //texture->mipMapCount = mipmapLevel + 1; // Mipmap level starts from 0
  texture->pitch = width * 4; // Assuming ARGB format (4 bytes per pixel)
  texture->offset = 0; // Offset in RSX memory, can be set later
  texture->cubemap = GCM_FALSE;

  texture->location = GCM_LOCATION_RSX;
  //texture->swizzleMode = GCM_TEXTURE_SWIZZLE_MODE_NONE;
  rsxAddressToOffset(imageData, &texture->offset);
  
  printf("Creating texture: %dx%d\n", width, height);
  if (rsxAddressToOffset(imageData, &texture->offset)) {
    printf("Failed to convert address to offset for texture: %dx%d\n", width, height);
    return;
  }

  rsxLoadTexture(context, 0, texture);
}

void* loadShader(const char* shaderFile){
  FILE* file = fopen(shaderFile, "rb");
  if (!file) {
    printf("Failed to open vertex program file\n");
    return;
  }
  
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  void* shaderData = rsxMemalign(size,16);
  if (!shaderData) {
    printf("Failed to allocate memory for vertex program\n");
    fclose(file);
    return;
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
    return;
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

  //rsxSetVertexAttribPointer(context, 0, 4, GCM_TRUE, sizeof(Vertex), (void*)offset);
  rsxBindVertexArrayAttrib(context, 0, offset, sizeof(Vertex), 4,1,0,0); // Position
  rsxBindVertexArrayAttrib(context, 1, offset + offsetof(Vertex, u), sizeof(Vertex), 2, 1, 1, 0); // UV as float
  
  // Can't just push it as a string, it needs to be actually loaded into memory.
  // We need to write a vertex program to handle the vertex data... Whatever the hell that is.
  
  rsxDrawVertexArray(context, GCM_TYPE_TRIANGLES, 0, 6);
  rsxSetSurface(context, &testSurface); // Set the surface to draw on

  //rsxBindTexture(context, 0, &backgroundTexture); // Bind the texture to the RSX context

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
  shader = loadShader("/dev_hdd0/V4.20/simple.vp");
  fragShader = loadShader("/dev_hdd0/V4.20/fragShader.fp");
  createTexture(&backgroundTexture, imageData, IMAGE_WIDTH, IMAGE_HEIGHT); // Need to return imageData and pass it into here.
  createQuad(); // Create the quad to draw the image on the screen.
  rsxLoadVertexProgram(context, &vertexProgram, shader);
  //rsxLoadFragmentProgram(context, &fragmentProgram, fragShader);
  // void* imageData = load_raw_argb(BG_PATH + "bedroom.raw", width, height); // Nice try, this would work in Python but not C.
   // This should draw a 1280 x 720 quad.
  /* Allocate a 1Mb buffer, aligned to a 1Mb boundary                          
   * to be our shared IO memory with the RSX. */
  
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
	ioPadGetData(i, &paddata);
				
	if(paddata.BTN_START){
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
