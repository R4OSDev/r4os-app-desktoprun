#include <r4os/app_gui.h>
int32_t r4_app_main(R4App *app) {
 uint64_t argument_length=0;const uint8_t*arguments=r4_app_args(app,&argument_length);
 const int empty=argument_length==5&&arguments[0]=='E',fast=argument_length==4&&arguments[0]=='F';
 if(argument_length==5&&arguments[0]=='T') {uint8_t bytes[1024];for(uint32_t i=0;i<sizeof bytes;i++)bytes[i]='A';for(uint32_t i=0;i<20;i++)((R4SysWriteFn)(uintptr_t)app->system.table->write)(bytes,sizeof bytes);}
 if(fast){
  static const uint8_t a[]="OBS26 STDOUT BEGIN\nOBS26 STDOUT END\n",b[]="OBS26 STDERR BEGIN\nOBS26 STDERR END\n";
  ((R4SysWriteFn)(uintptr_t)app->system.table->write)(a,sizeof a-1);((R4DeskConsoleWriteFn)(uintptr_t)app->desktop.table->console_write)(R4ABI_CONSOLE_STREAM_STDERR,b,sizeof b-1);return 37;
 }
 R4Window w;if(!r4_window_open(app,0,0,&w))return 91;
 static const uint8_t begin[]="OBS26 STDOUT BEGIN\n";
 int32_t written=empty?0:((R4SysWriteFn)(uintptr_t)app->system.table->write)(begin,sizeof begin-1);
 static const uint8_t errbegin[]="OBS26 STDERR BEGIN\n";if(!empty)((R4DeskConsoleWriteFn)(uintptr_t)app->desktop.table->console_write)(R4ABI_CONSOLE_STREAM_STDERR,errbegin,sizeof errbegin-1);
 if(argument_length==5&&arguments[0]=='C'){static const uint8_t clear[]={12};((R4SysWriteFn)(uintptr_t)app->system.table->write)(clear,1);}
 if(r4_window_set_title(&w,written>=0?"OBS26 ready":"OBS26 write failed")<0)return 92;
 R4PaintContext p;if(!r4_window_begin_paint(&w,&p))return 93;
 R4Canvas c=r4_paint_canvas(&p);if(r4_canvas_clear(c,0x182030)<0||r4_canvas_text(c,8,8,"OBS26 output and exit witness",0xffffff,0x182030)<0||r4_paint_present(&p)<0)return 94;
 for(;;){R4MessageNext n=r4_window_wait_message(&w,r4_timeout_forever());if(n.state==R4_MESSAGE_NEXT_FAILED)return 95;
 if(n.state==R4_MESSAGE_NEXT_MESSAGE&&n.message.kind==R4_MESSAGE_CLOSE){
  static const uint8_t end[]="OBS26 STDOUT END\n";if(!empty)((R4SysWriteFn)(uintptr_t)app->system.table->write)(end,sizeof end-1);
  static const uint8_t errend[]="OBS26 STDERR END\n";if(!empty)((R4DeskConsoleWriteFn)(uintptr_t)app->desktop.table->console_write)(R4ABI_CONSOLE_STREAM_STDERR,errend,sizeof errend-1);return 37;
 }}
}
