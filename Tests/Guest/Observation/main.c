#include <r4os/app_gui.h>
#include <r4os/app_resources.h>
#include <stdatomic.h>
static void text(R4App*a,const char*s){uint32_t n=0;while(s[n])n++;((R4SysWriteFn)(uintptr_t)a->system.table->write)((const uint8_t*)s,n);}
static void number(R4App*a,int64_t v){uint8_t b[24];uint32_t n=0;uint64_t u=v<0?(uint64_t)(-(v+1))+1:(uint64_t)v;if(v<0)text(a,"-");do{b[n++]=(uint8_t)('0'+u%10);u/=10;}while(u);while(n)((R4SysWriteFn)(uintptr_t)a->system.table->write)(&b[--n],1);}
static void field(R4App*a,const char*k,int64_t v){text(a,k);number(a,v);}
typedef struct {R4App*app;R4ProgramObservationHandle key;_Atomic int entered,result;R4ProgramProcessCompletion completion;} SharedWait;
static int32_t wait_thread(uint64_t argument){
 SharedWait*s=(SharedWait*)(uintptr_t)argument;R4Resources r={.app=s->app};atomic_store_explicit(&s->entered,1,memory_order_release);
 int32_t result=r4_resources_observation_wait(&r,&s->key,r4_timeout_forever(),&s->completion);
 atomic_store_explicit(&s->result,result,memory_order_release);return 0;
}
int32_t r4_app_main(R4App*a){
 uint64_t len=0;const uint8_t*args=r4_app_args(a,&len);if(len<2)return 2;uint8_t mode=args[0];args++;len--;
 uint64_t values[3]={0};uint32_t f=0,digits=0;for(uint64_t i=0;i<len;i++){uint8_t c=args[i];if(c>='0'&&c<='9'){if(values[f]>(UINT64_MAX-(c-'0'))/10)return 2;values[f]=values[f]*10+c-'0';digits++;}else if(c==':'&&digits&&f<2){f++;digits=0;}else return 2;}
 if(f!=2||!digits||values[0]>UINT32_MAX||values[2]>INT32_MAX)return 2;
 R4ProgramProcessHandle h={.instance_id=(uint32_t)values[0],.generation=values[1]};R4ProgramInstanceInfo info={0};
 int32_t status=((R4SysProgramHandleStatusFn)(uintptr_t)a->system.table->program_handle_status)(&h,&info);
 uint8_t output[256]={0};int32_t bytes=((R4DeskConsoleOutputFn)(uintptr_t)a->desktop.table->console_output)(h.instance_id,output,sizeof output);
 field(a,"OBS26 live status=",status);field(a," state=",info.state);field(a," console-bytes=",bytes);text(a,"\n");
 R4Resources resources={.app=a};
 if(mode=='W'||mode=='V'){
  R4ProgramObservationHandle key={0},second={0};int32_t held=r4_resources_observation_retain(&resources,&h,0,&key);if(held)return 8;
  SharedWait shared={.app=a,.key=key,.result=INT32_MAX};R4JoinHandle thread={0};int32_t created=r4_resources_create_thread(&resources,wait_thread,(uintptr_t)&shared,1048576,&thread);if(created)return 9;
  while(!atomic_load_explicit(&shared.entered,memory_order_acquire))((R4SysSleepTicksFn)(uintptr_t)a->system.table->sleep_ticks)(1);
  int32_t local=0;
  if(mode=='W')local=r4_resources_observation_release(&resources,&key);
  else {
   if(r4_resources_observation_retain(&resources,&h,0,&second))return 10;
   R4GuiEvent event={.kind=R4_GUI_RAW_EVENT_CLOSE,.window_id=(int32_t)values[2]};local=((R4DeskGuiPushEventFn)(uintptr_t)a->desktop.table->gui_push_event)(h.instance_id,&event);
   R4ProgramProcessCompletion c={0};local=local?local:r4_resources_observation_wait(&resources,&second,r4_timeout_forever(),&c);
   field(a,"OBS26 concurrent-main=",local);field(a," actual-exit=",c.exit_code);text(a,"\n");
  }
  R4ResourceWait joined=r4_join_wait(&thread,r4_timeout_forever());int32_t other=atomic_load_explicit(&shared.result,memory_order_acquire);
  field(a,"OBS26 concurrent-thread=",other);field(a," join-kind=",joined.kind);field(a," join-exit=",joined.value);field(a," local=",local);text(a,"\n");
  if(mode=='V'){
   ((R4SysSleepTicksFn)(uintptr_t)a->system.table->sleep_ticks)(1000);R4ProgramProcessCompletion old={0};
   field(a,"OBS26 consumed-owner-wait=",((R4SysProgramHandleWaitFn)(uintptr_t)a->system.table->program_handle_wait)(&h,0,&old));text(a,"\n");
   field(a,"OBS26 concurrent-thread-exit=",shared.completion.exit_code);field(a," release-a=",r4_resources_observation_release(&resources,&key));field(a," release-b=",r4_resources_observation_release(&resources,&second));text(a,"\n");}
  return joined.kind!=R4_RESOURCE_WAIT_EXITED||joined.value!=0||local!=0||other!=(mode=='W'?-2:0)?11:0;
 }
 if(mode=='D'||mode=='P'){
  R4ProgramObservationHandle o={0};int32_t result=r4_resources_observation_retain(&resources,&h,mode=='P'?R4OS_PROGRAM_OBSERVATION_FLAG_PORTABLE:0,&o);
  field(a,"OBS26 binding-result=",result);field(a," binding-id=",o.id);text(a,"\n");return result?5:0;
 }
 if(mode=='R'||mode=='T'){
  R4ProgramObservationHandle o={.process=h,.id=values[2]};int32_t retained=0;
  if(mode=='T'){R4ProgramProcessCompletion old={0};field(a,"OBS26 before-late-bind-owner-wait=",((R4SysProgramHandleWaitFn)(uintptr_t)a->system.table->program_handle_wait)(&h,0,&old));text(a,"\n");retained=r4_resources_observation_retain(&resources,&h,0,&o);}
  R4ProgramProcessCompletion c={0};int32_t result=retained?retained:r4_resources_observation_wait(&resources,&o,r4_timeout_poll(),&c);
  field(a,"OBS26 retained=",retained);field(a," observed=",result);if(!result){field(a," actual-exit=",c.exit_code);field(a," output-length=",c.output_length);field(a," flags=",c.flags);}text(a,"\n");
  uint32_t got=0;int32_t rd=r4_resources_observation_read(&resources,&o,c.output_length>sizeof output?c.output_length-sizeof output:0,output,sizeof output,&got);
  field(a,"OBS26 retained-read=",rd);field(a," bytes=",got);field(a," release=",r4_resources_observation_release(&resources,&o));text(a,"\n");
  if(rd==0)((R4SysWriteFn)(uintptr_t)a->system.table->write)(output,got);return 0;
 }
 if(mode=='K'){
  R4ProgramObservationHandle o={0};int32_t held=r4_resources_observation_retain(&resources,&h,0,&o);if(held)return 6;
  field(a,"OBS26 kill=",((R4SysProgramHandleKillFn)(uintptr_t)a->system.table->program_handle_kill)(&h));
  R4ProgramProcessCompletion c={0};int32_t result=r4_resources_observation_wait(&resources,&o,r4_timeout_forever(),&c);
  field(a," observed=",result);field(a," actual-exit=",c.exit_code);field(a," output-length=",c.output_length);field(a," flags=",c.flags);field(a," release=",r4_resources_observation_release(&resources,&o));text(a,"\n");return result?7:0;
 }
 if(mode=='G'){
  R4Resources resources={.app=a};R4ProgramObservationHandle first={0},second={0};
  int32_t one=r4_resources_observation_retain(&resources,&h,0,&first),two=r4_resources_observation_retain(&resources,&h,0,&second);
  field(a,"OBS26 retain-one=",one);field(a," retain-two=",two);text(a,"\n");if(one||two)return 3;
  R4ProgramProcessCompletion pending={0};field(a,"OBS26 pending=",r4_resources_observation_wait(&resources,&first,r4_timeout_poll(),&pending));text(a,"\n");
  R4GuiEvent e={.kind=R4_GUI_RAW_EVENT_CLOSE,.window_id=(int32_t)values[2]};int32_t push=((R4DeskGuiPushEventFn)(uintptr_t)a->desktop.table->gui_push_event)(h.instance_id,&e);field(a,"OBS26 close-push=",push);text(a,"\n");
  R4ProgramProcessCompletion completion={0};int32_t wait=r4_resources_observation_wait(&resources,&first,r4_timeout_forever(),&completion);
  field(a,"OBS26 first-wait=",wait);if(wait>=0){field(a," actual-exit=",completion.exit_code);field(a," output-length=",completion.output_length);field(a," flags=",completion.flags);}text(a,"\n");
  uint32_t got=0;int32_t read=r4_resources_observation_read(&resources,&first,0,output,sizeof output,&got);field(a,"OBS26 first-read=",read);field(a," bytes=",got);text(a,"\n");if(read==0)((R4SysWriteFn)(uintptr_t)a->system.table->write)(output,got);
  ((R4SysSleepTicksFn)(uintptr_t)a->system.table->sleep_ticks)(1000);
  R4ProgramProcessCompletion after={0};wait=r4_resources_observation_wait(&resources,&second,r4_timeout_poll(),&after);
  field(a,"OBS26 post-reap-wait=",wait);field(a," actual-exit=",after.exit_code);field(a," first-release=",r4_resources_observation_release(&resources,&first));text(a,"\n");
  read=r4_resources_observation_read(&resources,&second,0,output,sizeof output,&got);field(a,"OBS26 second-read=",read);field(a," bytes=",got);text(a,"\n");
  R4ProgramObservationHandle wrong=second;wrong.process.generation++;field(a,"OBS26 wrong-generation=",r4_resources_observation_wait(&resources,&wrong,r4_timeout_poll(),&pending));
  field(a," last-release=",r4_resources_observation_release(&resources,&second));field(a," rebind-after-last=",r4_resources_observation_retain(&resources,&h,0,&first));text(a,"\n");
  if(wait||read||after.exit_code!=37)return 4;
 }

 R4ProgramProcessCompletion late={0};int32_t wait=((R4SysProgramHandleWaitFn)(uintptr_t)a->system.table->program_handle_wait)(&h,0,&late);uint32_t got=0;
 int32_t read=((R4SysProgramCompletionReadFn)(uintptr_t)a->system.table->program_completion_read)(&h,0,output,sizeof output,&got);
 field(a,"OBS26 late-wait=",wait);if(wait>=0){field(a," actual-exit=",late.exit_code);field(a," flags=",late.flags);}field(a," late-read=",read);field(a," bytes=",got);text(a,"\n");return 0;
}
