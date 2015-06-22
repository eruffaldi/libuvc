/**
 * Real Sense Example (Emanuele Ruffaldi 2015, from original example)

 - added more parameters
 - added more save
 - added more stability
 */
#include "libuvc/libuvc.h"
#include <stdio.h>
#include <cv.h>
#include <highgui.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

pthread_cond_t done[4];
pthread_cond_t request;
pthread_mutex_t lock;
IplImage * pending[4];
const char * names[] = {"Color","Depth16","Depth8","Depth+IR"};
 char depthmode2code[]= {"IDX"};
int depths[] = {UVC_FRAME_FORMAT_INVI,UVC_FRAME_FORMAT_INVR,UVC_FRAME_FORMAT_INRI};


void dispatchimage(int id, IplImage * img)
{
  fprintf(stdout,"dispatchimage %d %p\n",id,img);
  fflush(stdout);
  pending[id] = img;
  pthread_cond_signal(&request);
  pthread_mutex_lock(&lock);
  while(pending[id])
    pthread_cond_wait(&done[id],&lock);
  pthread_mutex_unlock(&lock);   
  fprintf(stdout,"dispatchimage back\n");
  fflush(stdout);
}

int sigintreq = 0;

void my_handler(int s)
{
           printf("Caught signal %d\n",s);
           sigintreq = 1;
}

void loopimage()
{

   struct sigaction sigIntHandler;

   sigIntHandler.sa_handler = my_handler;
   sigemptyset(&sigIntHandler.sa_mask);
   sigIntHandler.sa_flags = 0;

   sigaction(SIGINT, &sigIntHandler, NULL);

  while(1)
  {
    /*
    struct timespec ts;
    struct timeval    tp;
    int rc =  gettimeofday(&tp, NULL);
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;
    ts.tv_sec += 0;
    ts.tv_nsec = waitms*1000000;
    */

      pthread_mutex_lock(&lock);
      pthread_cond_wait(&request,&lock);
      if(sigintreq)
      {
        pthread_mutex_unlock(&lock);
        break;
      }
      fprintf(stdout,"main awaken\n");
      fflush(stdout);
      for(int i = 0; i < 4; i++)
      {
          if(pending[i])
          {
            fprintf(stderr,"main show: %s %p\n",names[i],pending[i]);
            //cvNamedWindow(names[i], CV_WINDOW_AUTOSIZE);
            //cvShowImage(names[i], pending[i]);
            pending[i] = 0;         
//            pthread_cond_signal(&done[i]);
          }
      }      
      pthread_mutex_unlock(&lock);
      //cvWaitKey(10);
  }
}

/* This callback function runs once per frame. Use it to perform any
 * quick processing you need, or have it put the frame into your application's
 * input queue. If this function takes too long, you'll start losing frames. */
void cb_rgb(uvc_frame_t *frame, void *ptr) {
  uvc_frame_t *bgr;
  uvc_error_t ret;

  /* We'll convert the image from YUV/JPEG to BGR, so allocate space */
  bgr = uvc_allocate_frame(frame->width * frame->height * 3);
  if (!bgr) {
    printf("unable to allocate bgr frame!");
    return;
  }

  /* Do the BGR conversion */
  ret = uvc_any2bgr(frame, bgr);
  if (ret) {
    uvc_perror(ret, "uvc_any2bgr");
    uvc_free_frame(bgr);
    return;
  }

  /* Call a user function:
   *
   * my_type *my_obj = (*my_type) ptr;
   * my_user_function(ptr, bgr);
   * my_other_function(ptr, bgr->data, bgr->width, bgr->height);
   */

  /* Call a C++ method:
   *
   * my_type *my_obj = (*my_type) ptr;
   * my_obj->my_func(bgr);
   */

  //printf("RGB frame time: %9d.%06d\n", frame->capture_time.tv_sec, frame->capture_time.tv_usec);
  uint64_t scr;
  memcpy(&scr, &frame->capture_time, sizeof(scr));
  //printf("RGB frame time: %lu\n", scr);
  
  /* Use opencv.highgui to display the image: */
  IplImage* cvImg;
    cvImg = cvCreateImageHeader(
        cvSize(bgr->width, bgr->height),
        IPL_DEPTH_8U,
        3);
    cvSetData(cvImg, bgr->data, bgr->width * 3); 
   
   dispatchimage(0,cvImg);
   
    cvReleaseImageHeader(&cvImg);
   

  uvc_free_frame(bgr);
}

/*Convert a YUYV -advertised image to a gray16 image for OpenCV */

uvc_error_t uvc_yuyv2gray16(uvc_frame_t *in) {
  if (in->frame_format != UVC_FRAME_FORMAT_YUYV)
    return UVC_ERROR_INVALID_PARAM;

  in->frame_format = UVC_FRAME_FORMAT_GRAY16;
  return UVC_SUCCESS;
}


//Callback for the 16-bit gray depthmap
void cb_gray16(uvc_frame_t *frame, void *ptr) {
  uvc_frame_t *gray;
  uvc_error_t ret;

  gray = frame;

  gray->frame_format = UVC_FRAME_FORMAT_GRAY16;

  int maxVal = 0;
  uint16_t *grayData = (uint16_t*) gray->data; 
  uint16_t theVal;
  int i;
  /* for (i = 0; i<gray->height*gray->width; i++) */
  /*   { */
  /*     theVal = grayData[i]; */
  /*     if (theVal > maxVal) */
  /* 	maxVal = theVal; */
  /*   } */

  //maxVal = grayData[240+480*320];
  //printf("Center depth: %1.3f\n", ((float)maxVal)/1000);
  //printf("Depth frame time: %d.%06d\n", frame->capture_time.tv_sec, frame->capture_time.tv_usec);
  uint64_t scr;
  memcpy(&scr, &frame->capture_time, sizeof(scr));
  //printf("Depth frame time: %lu\n", scr);
  
  /* Use opencv.highgui to display the image: */
  IplImage* cvImg;
    cvImg = cvCreateImageHeader(
        cvSize(gray->width, gray->height),
        IPL_DEPTH_16U,
        1);
   
    cvSetData(cvImg, gray->data, gray->width * 2); 
    dispatchimage(1,cvImg); 
    cvReleaseImageHeader(&cvImg);
}

//Callback for the 8-bit gray IR image
void cb_gray8(uvc_frame_t *frame, void *ptr) {
  uvc_frame_t *gray;
  uvc_error_t ret;

  gray = frame;

  gray->frame_format = UVC_FRAME_FORMAT_INVI;
  uint64_t scr;
  memcpy(&scr, &frame->capture_time, sizeof(scr));
  //printf("IR frame time: %lu\n", scr);
  
  /* Use opencv.highgui to display the image: */
  IplImage* cvImg;
    cvImg = cvCreateImageHeader(
        cvSize(gray->width, gray->height),
        IPL_DEPTH_8U,
        1);
   
    cvSetData(cvImg, gray->data, gray->width * 1); 
    dispatchimage(2,cvImg); 
   
    cvReleaseImageHeader(&cvImg);
}

//Callback for 24-bit image format
void cb_24(uvc_frame_t *frame, void *ptr) {
  uvc_frame_t *gray;
  uvc_error_t ret;
  uvc_frame_t *plane;
  plane = uvc_allocate_frame(frame->width * frame->height);
  plane->width = frame->width;
  plane->height = frame->height;
  gray = frame;

  gray->frame_format = UVC_FRAME_FORMAT_INRI;
  uint64_t scr;
  memcpy(&scr, &frame->capture_time, sizeof(scr));
  //printf("IR frame time: %lu\n", scr);

  int i,j;
  j = 0;
  uint8_t *planeData = (uint8_t*)plane->data;
  uint8_t *frameData = (uint8_t*)frame->data;
  for (i=0; i < frame->width*frame->height*3; i+= 3)
    {
      planeData[j] = frameData[i];
      j++;
    }
  /* Use opencv.highgui to display the image: */
  IplImage* cvImg;
    cvImg = cvCreateImageHeader(
        cvSize(plane->width, plane->height),
        IPL_DEPTH_8U,
        1);
   
    cvSetData(cvImg, plane->data, plane->width * 1); 
    dispatchimage(3,cvImg); 
   
    cvReleaseImageHeader(&cvImg);
    uvc_free_frame(plane);
}

int depthCount=0;

void cb_none(uvc_frame_t *frame, void *ptr) {
  fprintf(stdout,"%c",(char)ptr);
  fflush(stdout);
}

//Callback for the 16-bit gray depthmap - image writer
void cb_graypgm_file(uvc_frame_t *frame, void *ptr) {
  uvc_frame_t *gray;
  uvc_error_t ret;
  int i,j;
  int depthmode = (int)ptr;

  fprintf(stdout,"%c",depthmode2code[depthmode]);
  fflush(stdout);

  gray = frame;

  //NOT USEDgray->frame_format = depth == UVC_FRAME_FORMAT_GRAY16;

  char fileName[32];
  sprintf(fileName, "depth%d.pgm", depthCount++);
  FILE* outFile = fopen(fileName, "wb");
  if(!outFile)
  {
    printf("cannot open file\n");
    return;
  }
  int w = gray->width;
  int n = 65535;
  switch(depthmode)
  {
    case 0: w = gray->width; n = 255; break;
    case 1: w = gray->width; n = 65535; break;
    case 2: w = gray->width*3; n = 255; break;
  }
  fprintf(outFile, "P2\n%d %d\n%d\n",w,gray->height,n);
  
  if(depthmode == 1)
  {
      uint16_t *data = (uint16_t*)gray->data;
      for (i =0; i<gray->height; i++)
        {
          for (j=0; j<w; j++)
    	       fprintf(outFile, "%d ", data[i*w + j]);
          fprintf(outFile, "\n");
        }
  }
  else
  {
      uint8_t *data = (uint8_t*)gray->data;
      for (i =0; i<gray->height; i++)
        {
          for (j=0; j<w; j++)
             fprintf(outFile, "%d ", data[i*w + j]);
          fprintf(outFile, "\n");
        }

  }

  fclose(outFile);
}

const char * fmt2str(int fmt)
{
  switch(fmt)
  {
  case UVC_FRAME_FORMAT_UNCOMPRESSED: return "uncompressed";
  case  UVC_FRAME_FORMAT_COMPRESSED: return "compressed";
  case  UVC_FRAME_FORMAT_YUYV: return "yuyv";
  case  UVC_FRAME_FORMAT_UYVY: return "uyvy";
  case UVC_FRAME_FORMAT_BGR : return "bgr";
  case UVC_FRAME_FORMAT_RGB: return "rgb";
  case  UVC_FRAME_FORMAT_INVI: return "invi8";
  case UVC_FRAME_FORMAT_RELI: return "reli8";
  case UVC_FRAME_FORMAT_INVR: return "invr16"; 
  case UVC_FRAME_FORMAT_INVZ: return "invz16";
  case UVC_FRAME_FORMAT_INRI: return "inri24";
default: return "unknown";
  }

}


//Callback for the 16-bit gray depthmap - image writer
void cb_grayraw_file(uvc_frame_t *frame, void *ptr) {
  int depthmode = (int)ptr;

  fprintf(stdout,"%c",depthmode2code[depthmode]);
  fflush(stdout);


  //NOT USEDgray->frame_format = depth == UVC_FRAME_FORMAT_GRAY16;

  if(depthCount == 0)
  {
    printf("depth received w:%d h:%d by:%d fmt:%d (%s) bpp:%d\n",frame->width,frame->height,frame->data_bytes,frame->frame_format,fmt2str(frame->frame_format),frame->data_bytes*8/(frame->width*frame->height));
  }

  char fileName[32];
  sprintf(fileName, "depth%d.bin", depthCount++);
  FILE* outFile = fopen(fileName, "wb");
  if(!outFile)
  {
    printf("cannot open file\n");
    return;
  }
  fwrite(frame->data,1,frame->data_bytes,outFile);
  fclose(outFile);
}


void cb_rgbbgr_file(uvc_frame_t *frame, void *ptr) {
  int i,j;
  int depthmode = (int)ptr;
  static uvc_frame_t * bgr;
  static int frameCount = 0;
  int ret;
  fprintf(stdout,"C");
  fflush(stdout);

  if(frameCount == 0)
  {
    printf("rgb received w:%d h:%d by:%d fmt:%d (%s) bpp:%d\n",frame->width,frame->height,frame->data_bytes,frame->frame_format,fmt2str(frame->frame_format),frame->data_bytes*8/(frame->width*frame->height));
  bgr = uvc_allocate_frame(frame->width * frame->height * 3);

  }
  /* Do the BGR conversion */
  ret = uvc_any2bgr(frame, bgr);
  if(ret == UVC_ERROR_NOT_SUPPORTED)
  {
    printf("UNSUPPORTED conversion\n");
    fflush(stdout);
    return;
  }
  frame = bgr;

  char fileName[32];
  sprintf(fileName, "image%d.bin", frameCount++);
  FILE* outFile = fopen(fileName, "wb");
  if(!outFile)
  {
    printf("cannot open file\n");
    return;
  }
  fwrite(frame->data,1,frame->data_bytes,outFile);
  fclose(outFile);

}
    // 8 IR,16 depth, 24 IR+depth


void cb_rgbraw_file(uvc_frame_t *frame, void *ptr) {
  int i,j;
  int depthmode = (int)ptr;
  static int frameCount = 0;

  fprintf(stdout,"C");
  fflush(stdout);

  if(frameCount == 0)
  {
    printf("depth received w:%d h:%d by:%d fmt:%d (%s) bpp:%d\n",frame->width,frame->height,(int)frame->data_bytes,frame->frame_format,fmt2str(frame->frame_format),(int)frame->data_bytes*8/(frame->width*frame->height));
  }

  char fileName[32];
  sprintf(fileName, "image%d.bin", frameCount++);
  FILE* outFile = fopen(fileName, "wb");
  if(!outFile)
  {
    printf("cannot open file\n");
    return;
  }
  fwrite(frame->data,1,frame->data_bytes,outFile);
  fclose(outFile);

}
    // 8 IR,16 depth, 24 IR+depth


int main(int argc, char **argv) {
  uvc_context_t *ctx = 0;
  uvc_device_t *dev = 0;
  uvc_device_handle_t *devh_d=0;
  uvc_device_handle_t *devh_rgb=0;
  uvc_stream_ctrl_t ctrl_rgb;
  uvc_stream_ctrl_t ctrl_d;
  int doabort = 0;
  int dorgb = 1;
  int dodepth = 1;
  int info = 0;
  int tofile = 1;
  int depthmode = 0; // IR,Depth,IR+Depth


  for(int i = 1; i < argc; i++)
  {
    if(strcmp(argv[i],"norgb") == 0)
      dorgb = 0;
    else if(strcmp(argv[i],"info") == 0)
      info = 1;
    else if(strcmp(argv[i],"nodepth") == 0)
      dodepth = 0;
    else if(strcmp(argv[i],"tofile") == 0)
      tofile = 1;
    else if(strcmp(argv[i],"help") == 0)
    {
      printf("libuvc RealSense example (ER 2015): norgb|info|nodepth|tofile|help\n");
      return 0;
    }
  }



  uvc_error_t res;

  /* Initialize a UVC service context. Libuvc will set up its own libusb
   * context. Replace NULL with a libusb_context pointer to run libuvc
   * from an existing libusb context. */
  res = uvc_init(&ctx, NULL);

  for(int i = 0; i < 4; i++)
    pthread_cond_init(&done[i],0);
  pthread_cond_init(&request,0);
  pthread_mutex_init(&lock,0);

  if (res < 0) {
    uvc_perror(res, "uvc_init");
    return res;
  }

  puts("UVC initialized");


  /* Locates the first attached UVC device, stores in dev */
  res = uvc_find_device(
      ctx, &dev,
      0x8086, 0x0a66, NULL); /* filter devices: vendor_id, product_id, "serial_num" */

  if (res < 0) {
    uvc_perror(res, "uvc_find_device"); /* no devices found */
    doabort = 1;
  } else {
    puts("Device found");
  }


  if(!doabort && dorgb)
  {
    printf("RGB opening\n");
    res = uvc_open2(dev, &devh_rgb, 0); //Try to open camera 0 (RGB)
    if(res < 0)
    {
        uvc_perror(res, "RGB: uvc_open error"); /* unable to open device */
    doabort = 1;
    }
    else
    {
  
      printf("----RGB: uvc_print_diag\n");
      uvc_print_diag(devh_rgb, stdout);

      /* Try to negotiate a 640x480 30 fps YUYV stream profile */
      res = uvc_get_stream_ctrl_format_size(
          devh_rgb, &ctrl_rgb, /* result stored in ctrl */
          UVC_FRAME_FORMAT_YUYV, /* YUV 422, aka YUV 4:2:2. try _COMPRESSED */
    640, 480, 30 /* width, height, fps */
      );
      /* Print out the result */
      if(res >= 0)
      {
        printf("----RGB: uvc_print_stream_ctrl\n");
        uvc_print_stream_ctrl(&ctrl_rgb, stdout);
        printf("RGB: ready\n");
      }
      else 
      {
        uvc_perror(res, "failed uvc_get_stream_ctrl_format_size of RGB");
        doabort = 1;
      }
    }
  }

  // setup DEPTH
  if(!doabort && dodepth)
  {

    printf("DEPTH: opening\n");
    res = uvc_open2(dev, &devh_d, 1); //Try to open camera 1  (depth)
    if(res >= 0)
    {
      printf("DEPTH: uvc_print_diag\n");
      uvc_print_diag(devh_d, stdout);
      /* Try to negotiate a 640x480 30 fps YUYV stream profile */
      res = uvc_get_stream_ctrl_format_size(
          devh_d, &ctrl_d, /* result stored in ctrl */
          depths[depthmode], /* YUV 422, aka YUV 4:2:2. try _COMPRESSED */
    //UVC_FRAME_FORMAT_YUYV,
          640, 480, 30 /* width, height, fps */
      );

      if (res >= 0) {

        /* Print out the result */
        printf("----DEPTH: uvc_print_stream_ctrl\n");
        uvc_print_stream_ctrl(&ctrl_d, stdout);
        printf("----DEPTH: uvc_print_diag\n");
        uvc_print_diag(devh_d, stdout);    
      }
      else
      {
        doabort = 1;
        uvc_perror(res,"DEPTH: uvc_get_stream_ctrl_format_size error");
      }
    } // setup DEPTH
    else
    {
        uvc_perror(res, "DEPTH: uvc_open error"); /* device doesn't provide a matching stream */      
        doabort = 1;
    }
}


  fflush(stdout);
      

  if(!doabort)
  {
      /* Start the video stream. The library will call user function cb:
       *   cb(frame, (void*) 12345)
       */



 //     int res1 = devh_rgb ? uvc_start_streaming(devh_rgb, &ctrl_rgb, cb_rgb, 12345, 0) : 0;
   //   int res2 = devh_d   ? uvc_start_streaming(devh_d, &ctrl_d, cb_gray_file, 12345, 0) : 0;
       int res1 = 0,res2 = 0;
        if(devh_rgb)
        {
          if(info)
            res1 = uvc_start_streaming(devh_rgb, &ctrl_rgb, cb_none, (void*)(uintptr_t)'c', 0) ;
          else if(tofile)
            res1 = uvc_start_streaming(devh_rgb, &ctrl_rgb, cb_rgbbgr_file, 0, 0);
          else
            res1 = uvc_start_streaming(devh_rgb, &ctrl_rgb, cb_rgb, (void*)(uintptr_t)12345, 0);
        }
        if(devh_d)
        {
          if(info)
            res2 = uvc_start_streaming(devh_d, &ctrl_d, cb_none, (void*)(uintptr_t)depthmode2code[depthmode], 0) ;
          else if(tofile)
            res2 = uvc_start_streaming(devh_d, &ctrl_d, cb_grayraw_file, (void*)(uintptr_t)depthmode, 0);
          // TODO the one with opencv
        }
         

    	//res = uvc_start_streaming(devh_d, &ctrl_d, cb_gray8, 12345, 0);
    	//res = uvc_start_streaming(devh_d, &ctrl_d, cb_24, 12345, 0);
      if(res1 < 0)
        uvc_perror(res1, "RGB: failed start_streaming"); /* unable to start stream */
      if(res2 < 0)
        uvc_perror(res2, "Depth: failed start_streaming"); /* unable to start stream */

      if (res1 >= 0 && res2 >= 0) {
        printf("Streaming %d %d\n",res1,res2);

        //uvc_set_ae_mode(devh, 1); /* e.g., turn on auto exposure */
        loopimage();

        /* End the stream. Blocks until last callback is serviced */
        if(devh_rgb) uvc_stop_streaming(devh_rgb);
        if(devh_d) uvc_stop_streaming(devh_d);

        puts("Done streaming.");
      }
  } // stream

    /* Release our handle on the device */
    if(devh_d) uvc_close(devh_d);
    if(devh_rgb)       uvc_close(devh_rgb);

    puts("Device closed");

    /* Release the device descriptor */
    uvc_unref_device(dev);
  

  /* Close the UVC context. This closes and cleans up any existing device handles,
   * and it closes the libusb context if one was not provided. */
  uvc_exit(ctx);
  puts("UVC exited");

  return 0;
}

