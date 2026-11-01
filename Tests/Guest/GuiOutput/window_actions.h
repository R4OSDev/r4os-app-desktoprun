#ifndef DESKRUN_WM98_GUI_FIXTURE_H
#define DESKRUN_WM98_GUI_FIXTURE_H
/* Exercise the normal public event/paint path. No private frame ACK, timer,
 * service command or synthesized input stands in for the application's paint. */
static void wm98_gui_number(R4App *app,int32_t value){
 char bytes[16];uint32_t n=0;uint32_t v=value<0?(uint32_t)(-(int64_t)value):(uint32_t)value;
 if(value<0)r4sys_write_cstr(&app->system,"-");
 do{bytes[n++]=(char)('0'+v%10);v/=10;}while(v);
 while(n){char c[2]={bytes[--n],0};r4sys_write_cstr(&app->system,c);}
}
static int wm98_gui_paint(R4Window *window,uint32_t sequence,int32_t *width,int32_t *height){
 R4PaintContext paint;if(!r4_window_begin_paint(window,&paint))return 0;
 *width=paint.info.client_w;*height=paint.info.client_h;
 R4Canvas canvas=r4_paint_canvas(&paint);
 int32_t a=r4_canvas_clear(canvas,0x203040u+sequence*0x070503u);
 int32_t b=r4_canvas_rect(canvas,8,8,32,32,0xe0a040u);
 if(a<0||b<0||r4_paint_present(&paint)<0)return 0;
 r4sys_write_cstr(&window->app->system,"WM98 PAINT sequence=");wm98_gui_number(window->app,(int32_t)sequence);
 r4sys_write_cstr(&window->app->system," rgb=");wm98_gui_number(window->app,(int32_t)(0x203040u+sequence*0x070503u));
 r4sys_write_cstr(&window->app->system," width=");wm98_gui_number(window->app,*width);
 r4sys_write_cstr(&window->app->system," height=");wm98_gui_number(window->app,*height);
 r4sys_write_cstr(&window->app->system,"\n");return 1;
}
static int32_t wm98_app(R4App *app){
 R4Window window;if(!r4_window_open(app,0,0,&window))return 111;
 if(r4_window_set_title(&window,"WM98 public SDK paint")<0)return 112;
 uint32_t sequence=1;int32_t width=0,height=0;
 if(!wm98_gui_paint(&window,sequence,&width,&height))return 113;
 for(;;){
  R4MessageNext next=r4_window_wait_message(&window,r4_timeout_forever());
  if(next.state==R4_MESSAGE_NEXT_FAILED)return 114;
  if(next.state!=R4_MESSAGE_NEXT_MESSAGE)continue;
  if(next.message.kind==R4_MESSAGE_CLOSE)return 37;
  if(next.message.kind==R4_MESSAGE_RESIZE){
   if(!wm98_gui_paint(&window,++sequence,&width,&height))return 115;
  }
  if(next.message.kind==R4_MESSAGE_MOUSE&&next.message.value.mouse.action==R4_MOUSE_DOWN){
   r4sys_write_cstr(&app->system,"WM98 INPUT x=");wm98_gui_number(app,next.message.value.mouse.x);
   r4sys_write_cstr(&app->system," y=");wm98_gui_number(app,next.message.value.mouse.y);
   r4sys_write_cstr(&app->system," width=");wm98_gui_number(app,width);
   r4sys_write_cstr(&app->system," height=");wm98_gui_number(app,height);
   r4sys_write_cstr(&app->system,"\n");
  }
 }
}
#endif
