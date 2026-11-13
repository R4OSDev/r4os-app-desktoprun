#ifndef DESKRUN_WM98_CONTROLLER_FIXTURE_H
#define DESKRUN_WM98_CONTROLLER_FIXTURE_H
#include <r4os/desktop_actions.h>
static uint64_t wm98_ticks(R4App *app,uint64_t ms){
 uint64_t ticks=0;R4Duration duration={ms*1000000u};
 (void)r4_timeout_to_ticks(r4_timeout_finite(duration),r4_services_monotonic_hz(app),&ticks);return ticks;
}
static void wm98_pause(R4App *app){((R4SysSleepTicksFn)(uintptr_t)R4SYS_SLOT(&app->system, sleep_ticks))(wm98_ticks(app,10));}
static void wm98_key(R4App *app,const char *key,uint64_t value){h97_text(app,key);h97_number(app,value);}
static void wm98_dump(R4App *app,const char *stage,const R4DesktopActionReply *reply){
 const R4DesktopActionState *s=&reply->current;
 h97_text(app,"WM98 {\"stage\":\"");h97_text(app,stage);h97_text(app,"\"");
 wm98_key(app,",\"phase\":",reply->phase);wm98_key(app,",\"grant\":\"",reply->request.token.grant);
 wm98_key(app,"\",\"operation\":\"",reply->request.operation);wm98_key(app,"\",\"view\":\"",s->view_revision);
 wm98_key(app,"\",\"applied_view\":\"",reply->applied.view_revision);wm98_key(app,"\",\"frame_before\":\"",s->frame_before);
 wm98_key(app,"\",\"candidate\":\"",s->candidate_generation);wm98_key(app,"\",\"frame\":\"",s->frame_generation);
 wm98_key(app,"\",\"composition\":\"",s->composition_serial);wm98_key(app,"\",\"input\":\"",s->input_revision);
 wm98_key(app,"\",\"owner\":",s->window.owner.instance_id);wm98_key(app,",\"generation\":\"",s->window.owner.generation);
 wm98_key(app,"\",\"window_serial\":\"",s->window.serial);wm98_key(app,"\",\"window\":",s->window.window_id);
 wm98_key(app,",\"width\":",(uint32_t)s->info.client_w);wm98_key(app,",\"height\":",(uint32_t)s->info.client_h);
 wm98_key(app,",\"x\":",(uint32_t)s->info.client_x);wm98_key(app,",\"y\":",(uint32_t)s->info.client_y);
 wm98_key(app,",\"proof\":",s->proof_flags);wm98_key(app,",\"ready\":",r4_desktop_action_frame_ready(reply));h97_text(app,"}\n");
}
static int wm98_save(R4App *app,const R4DesktopActionToken *token){
 R4Files files=r4_app_files(app);R4FilePath path;if(!h97_path("C:\\TEMP\\W98.TOK",&path))return 0;
 R4Transfer w=r4_files_write(&files,&path,(const uint8_t*)token,sizeof(*token));
 R4DesktopActionToken check={0};R4Transfer r=r4_files_read(&files,&path,(uint8_t*)&check,sizeof(check));
 if(w.state!=R4_TRANSFER_BYTES||w.bytes!=sizeof(*token)||r.state!=R4_TRANSFER_BYTES||r.bytes!=sizeof(check))return 0;
 const uint8_t *a=(const uint8_t*)token,*b=(const uint8_t*)&check;for(uint32_t i=0;i<sizeof(check);i++)if(a[i]!=b[i])return 0;return 1;
}
static int wm98_load(R4App *app,R4DesktopActionToken *token){
 R4Files files=r4_app_files(app);R4FilePath path;if(!h97_path("C:\\TEMP\\W98.TOK",&path))return 0;
 R4Transfer r=r4_files_read(&files,&path,(uint8_t*)token,sizeof(*token));return r.state==R4_TRANSFER_BYTES&&r.bytes==sizeof(*token)&&token->version==1&&token->size==sizeof(*token);
}
static char *wm98_decimal(char *out,uint64_t n){char reverse[24];uint32_t size=0;do{reverse[size++]=(char)('0'+n%10);n/=10;}while(n);while(size)*out++=reverse[--size];return out;}
static int wm98_wait(R4App *app,R4DesktopActionToken *token,uint64_t operation,R4DesktopActionReply *out,int frame){
 uint64_t deadline=r4_app_ticks(app)+wm98_ticks(app,5000);
 do{R4DesktopActionReply value={0};int32_t rc=r4_desktop_action_status(app,token,operation,&value);
  if(rc){wm98_key(app,"WM98 transport magnitude=",(uint64_t)(-(int64_t)rc));h97_text(app,"\n");return 0;}
  if(value.request.token.grant)*token=value.request.token;
  if(value.result==0&&(!frame||r4_desktop_action_frame_ready(&value))&&value.current.view_revision){*out=value;return 1;}
  if(value.result<0&&value.result!=R4OS_DESKTOP_ACTION_RESULT_PENDING&&value.result!=R4OS_DESKTOP_ACTION_RESULT_UNKNOWN){wm98_key(app,"WM98 domain magnitude=",(uint64_t)(-(int64_t)value.result));wm98_dump(app,"failed",&value);return 0;}
  wm98_pause(app);
 }while(r4_app_ticks(app)<deadline);return 0;
}
static int wm98_apply(R4App *app,R4DesktopActionToken *token,uint64_t operation,uint32_t kind,int32_t width,int32_t height,R4DesktopActionReply *out){
 R4DesktopActionRequest q=r4_desktop_action_request(token);q.operation=operation;q.kind=kind;if(kind==R4OS_DESKTOP_ACTION_KIND_RESIZE){q.x=32;q.y=72;}q.width=width;q.height=height;
 q.deadline=r4_app_ticks(app)+wm98_ticks(app,2000);
 /* Deliberately discard the original result. Status recovers this exact
  * operation; there is no replacement launch, action or renewed deadline. */
 R4DesktopActionReply discarded={0};if(r4_desktop_action_command(app,&q,&discarded))return 0;
 if(!wm98_wait(app,token,operation,out,1))return 0;
 wm98_dump(app,"applied",out);
 R4DesktopActionReply duplicate={0};if(r4_desktop_action_command(app,&q,&duplicate)||duplicate.result||duplicate.phase!=R4OS_DESKTOP_ACTION_PHASE_APPLIED||duplicate.applied.view_revision!=out->applied.view_revision||duplicate.applied_tick!=out->applied_tick)return 0;
 ++q.deadline;R4DesktopActionReply changed={0};if(r4_desktop_action_command(app,&q,&changed)||changed.result!=R4OS_DESKTOP_ACTION_RESULT_STALE)return 0;
 R4DesktopActionReply after={0};if(r4_desktop_action_status(app,token,operation,&after)||after.current.view_revision!=out->current.view_revision||after.current.frame_generation!=out->current.frame_generation)return 0;
 return 1;
}
/* Exercise the real response-capacity boundary, not a fabricated service
 * result. The kernel consumes this reply but copies no incomplete payload. */
static int wm98_short_reply(R4App *app,uint16_t op,const R4DesktopActionRequest *q){
 R4Services services=r4_app_services(app);R4ServiceConnection connection={0};
 if(r4_services_open(&services,R4OS_WINDOW_SERVICE_NAME,&connection))return 0;
 uint8_t small[8];for(uint32_t i=0;i<sizeof(small);i++)small[i]=0xA5;
 R4Duration duration={500000000};
 R4ServiceCallResult call=r4_service_connection_call(&connection,op,q,sizeof(*q),small,sizeof(small),r4_timeout_finite(duration));
 if(r4_service_connection_close(&connection))return 0;
 if(call.kind!=R4_SERVICE_CALL_FAILED||call.raw_code!=R4OS_SERVICE_API_RESULT_BUFFER_TOO_SMALL||call.header.payload_len!=sizeof(R4DesktopActionReply)||call.header.status||call.header.op!=op)return 0;
 for(uint32_t i=0;i<sizeof(small);i++)if(small[i]!=0xA5)return 0;
 wm98_key(app,"WM98 actual-small-reply op=",op);wm98_key(app," required=",call.header.payload_len);
 wm98_key(app," capacity=",sizeof(small));h97_text(app," no-payload-copied PASS\n");return 1;
}
static int32_t wm98_partial(R4App *app,R4DesktopActionToken *token){
 R4DesktopActionRequest q=r4_desktop_action_request(token);q.operation=17;q.kind=R4OS_DESKTOP_ACTION_KIND_RESIZE;
 q.x=32;q.y=72;q.width=300;q.height=180;q.deadline=r4_app_ticks(app)+wm98_ticks(app,2000);
 if(!wm98_short_reply(app,R4OS_DESKTOP_ACTION_OP_COMMAND,&q))return 147;
 R4DesktopActionReply applied={0};if(!wm98_wait(app,token,17,&applied,1))return 148;
 if(applied.current.info.client_w!=300||applied.current.info.client_h!=180||!r4_desktop_action_same_window(applied.current.window,token->window))return 149;
 wm98_dump(app,"partial-command-recovered",&applied);
 R4DesktopActionRequest status=r4_desktop_action_request(token);status.operation=17;
 if(!wm98_short_reply(app,R4OS_DESKTOP_ACTION_OP_STATUS,&status))return 150;
 R4DesktopActionReply after={0};if(r4_desktop_action_status(app,token,17,&after)||after.result||!r4_desktop_action_frame_ready(&after)||after.applied_tick!=applied.applied_tick||after.applied.view_revision!=applied.applied.view_revision||after.current.view_revision!=applied.current.view_revision||after.current.frame_generation!=applied.current.frame_generation)return 151;
 wm98_dump(app,"partial-status-recovered",&after);
 /* No COMMAND retry: recover the single original operation only by STATUS. */
 R4Resources resources={.app=app};R4ProgramObservationHandle binding={0};
 if(r4_resources_observation_retain(&resources,&token->window.owner,0,&binding))return 152;
 R4GuiEvent event={.kind=R4_GUI_RAW_EVENT_CLOSE,.window_id=(int32_t)token->window.window_id};
 if(((R4DeskGuiPushEventFn)(uintptr_t)R4DESK_SLOT(&app->desktop, gui_push_event))(token->window.owner.instance_id,&event)<0){(void)r4_resources_observation_release(&resources,&binding);return 153;}
 R4ProgramProcessCompletion completion={0};R4Duration duration={2000000000};
 int32_t done=r4_resources_observation_wait(&resources,&binding,r4_timeout_finite(duration),&completion);
 if(done||completion.exit_code!=37){(void)r4_resources_observation_release(&resources,&binding);return 154;}
 h97_text(app,"WM98 PARTIAL OUTPUT BEGIN\n");uint64_t offset=0;
 while(offset<completion.output_length){uint32_t got=0;uint8_t chunk[512];
  if(r4_resources_observation_read(&resources,&binding,offset,chunk,sizeof(chunk),&got)||!got){(void)r4_resources_observation_release(&resources,&binding);return 155;}
  ((R4SysWriteFn)(uintptr_t)R4SYS_SLOT(&app->system, write))(chunk,got);offset+=got;
 }
 if(r4_resources_observation_release(&resources,&binding))return 156;
 R4DesktopActionReply released={0};if(r4_desktop_action_release(app,token,&released)||released.result!=R4OS_DESKTOP_ACTION_RESULT_RELEASED||released.phase!=R4OS_DESKTOP_ACTION_PHASE_RELEASED)return 157;
 h97_text(app,"WM98 PARTIAL PASS one-original-command two-actual-small-replies status-only-recovery close37 observation-release named-controller-release\n");return 0;
}
static int32_t wm98_controller(R4App *app,const uint8_t *args,uint64_t len){
 if(len==1&&args[0]=='E'){
  R4Files files=r4_app_files(app);R4FilePath path;if(!h97_path("C:\\TEMP\\OLD98.TOK",&path))return 142;
  R4DesktopActionToken old={0};R4Transfer read=r4_files_read(&files,&path,(uint8_t*)&old,sizeof(old));
  if(read.state!=R4_TRANSFER_BYTES||read.bytes!=sizeof(old)||old.version!=1||old.size!=sizeof(old))return 143;
  R4DesktopActionReply scope={0},status={0};if(r4_desktop_action_scope(app,&scope)||scope.result)return 144;
  for(uint32_t i=0;i<3;i++)if(scope.request.token.epoch[i]!=old.epoch[i])return 145;
  if(r4_desktop_action_status(app,&old,0,&status)||status.result>=0||status.current.view_revision||r4_desktop_action_frame_ready(&status))return 146;
  wm98_key(app,"WM98 old-token rejected domain-magnitude=",(uint64_t)(-(int64_t)status.result));h97_text(app," same-service-epoch current-view0 PASS\n");return 0;
 }
 if(len==1&&(args[0]=='R'||args[0]=='X'||args[0]=='K')){
  R4DesktopActionToken token={0};if(!wm98_load(app,&token))return 121;
  R4DesktopActionReply reply={0};if(r4_desktop_action_status(app,&token,args[0]=='K'?0:4,&reply)||reply.result)return 122;wm98_dump(app,"other-observer",&reply);
  R4DesktopActionRequest q=r4_desktop_action_request(&token);q.operation=999;q.kind=R4OS_DESKTOP_ACTION_KIND_RESIZE;q.width=400;q.height=300;q.deadline=r4_app_ticks(app)+wm98_ticks(app,1000);
  R4DesktopActionReply denied={0};int32_t rc=r4_desktop_action_command(app,&q,&denied);if(rc||denied.result!=R4OS_DESKTOP_ACTION_RESULT_NOT_OWNER)return 123;
  h97_text(app,"WM98 foreign-controller denied\n");
  uint8_t output[512];int32_t bytes=((R4DeskConsoleOutputFn)(uintptr_t)R4DESK_SLOT(&app->desktop, console_output))(token.window.owner.instance_id,output,sizeof(output));if(bytes>0)((R4SysWriteFn)(uintptr_t)R4SYS_SLOT(&app->system, write))(output,(uint32_t)bytes);
  if(args[0]=='X'||args[0]=='K'){
   R4Resources resources={.app=app};R4ProgramObservationHandle binding={0};
   if(r4_resources_observation_retain(&resources,&token.window.owner,0,&binding))return 138;
   R4GuiEvent event={.kind=R4_GUI_RAW_EVENT_CLOSE,.window_id=(int32_t)token.window.window_id};
   if(((R4DeskGuiPushEventFn)(uintptr_t)R4DESK_SLOT(&app->desktop, gui_push_event))(token.window.owner.instance_id,&event)<0){(void)r4_resources_observation_release(&resources,&binding);return 124;}
   R4ProgramProcessCompletion completion={0};R4Duration duration={2000000000};
   int32_t done=r4_resources_observation_wait(&resources,&binding,r4_timeout_finite(duration),&completion);
   if(done||completion.exit_code!=37){(void)r4_resources_observation_release(&resources,&binding);return 139;}
   h97_text(app,"WM98 OUTPUT BEGIN\n");uint64_t offset=0;
   while(offset<completion.output_length){uint32_t got=0;uint8_t chunk[512];
    int32_t rd=r4_resources_observation_read(&resources,&binding,offset,chunk,sizeof(chunk),&got);
    if(rd||!got){(void)r4_resources_observation_release(&resources,&binding);return 140;}
    ((R4SysWriteFn)(uintptr_t)R4SYS_SLOT(&app->system, write))(chunk,got);offset+=got;
   }
   h97_text(app,"WM98 OUTPUT END exit37\n");if(r4_resources_observation_release(&resources,&binding))return 141;
  }
  return 0;
 }
 const int bind_only=len==1&&args[0]=='N',partial_only=len==1&&args[0]=='P';if(len&&!bind_only&&!partial_only)return 125;
 R4ProgramProcessHandle self={0},helper={0};if(((R4SysProgramOpenHandleFn)(uintptr_t)R4SYS_SLOT(&app->system, program_open_handle))((uint32_t)r4_app_instance_id(app),&self))return 126;
 R4DesktopActionReply scoped={0};if(r4_desktop_action_scope(app,&scoped)||scoped.result)return 127;
 char command[160]="/JSON /WAIT:5000 /WAITFRAME /CONTROLLER:";char *p=command;while(*p)++p;p=wm98_decimal(p,self.instance_id);*p++=':';p=wm98_decimal(p,self.generation);
 const char *tail=" C:\\TEMP\\G98.R4X WM98";while(*tail)*p++=*tail++;*p=0;
 if(((R4SysProgramSpawnHandleFn)(uintptr_t)R4SYS_SLOT(&app->system, program_spawn_handle))((const uint8_t*)"C:\\R4OS\\SOFTWARE\\TERMINAL\\DESKRUN.R4X",(const uint8_t*)command,0,&helper))return 128;
 R4DesktopActionToken token=scoped.request.token;token.request=helper;token.controller=self;token.rights=R4OS_DESKTOP_ACTION_RIGHT_ALL;
 R4DesktopActionReply original={0};if(!wm98_wait(app,&token,0,&original,0)){h97_text(app,"WM98 initial grant/view lookup failed\n");return 129;}
 if(!token.grant||!token.window.serial){h97_text(app,"WM98 initial canonical token unbound\n");return 129;}
 if(!wm98_save(app,&token)){h97_text(app,"WM98 initial token persistence failed\n");return 129;}
 // The original DESKRUN may finish while the named controller and GUI live.
 // Observe its exact handle without consuming completion or racing Desktop reap.
 uint64_t helper_deadline=r4_app_ticks(app)+wm98_ticks(app,1000);int helper_ended=0;
 do {R4ProgramInstanceInfo info={0};int32_t status=((R4SysProgramHandleStatusFn)(uintptr_t)R4SYS_SLOT(&app->system, program_handle_status))(&helper,&info);
  if((status==0&&info.state==2)||status==R4OS_PROGRAM_HANDLE_ERROR_NOT_FOUND){helper_ended=1;break;}wm98_pause(app);
 }while(r4_app_ticks(app)<helper_deadline);
 if(!helper_ended)return 137;
 h97_text(app,"WM98 original DESKRUN ended; original named-controller grant retained\n");
 wm98_dump(app,"original",&original);
 if(bind_only){h97_text(app,"WM98 bind-only canonical original target PASS\n");return 0;}
 if(partial_only)return wm98_partial(app,&token);
 R4DesktopActionReply resized={0},clamped={0},minimized={0},restored={0};
 if(!wm98_apply(app,&token,1,R4OS_DESKTOP_ACTION_KIND_RESIZE,320,200,&resized)||resized.current.info.client_w!=320||resized.current.info.client_h!=200)return 130;
 if(!wm98_apply(app,&token,2,R4OS_DESKTOP_ACTION_KIND_RESIZE,1,1,&clamped)||clamped.current.info.client_w<=1||clamped.current.info.client_h<=1)return 131;
 if(!wm98_apply(app,&token,3,R4OS_DESKTOP_ACTION_KIND_MINIMIZE,0,0,&minimized)||!(minimized.current.proof_flags&R4OS_DESKTOP_ACTION_PROOF_INVISIBLE)||minimized.current.frame_generation)return 132;
 if(!wm98_apply(app,&token,4,R4OS_DESKTOP_ACTION_KIND_RESTORE,0,0,&restored)||!r4_desktop_action_same_window(original.current.window,restored.current.window)||restored.current.view_revision<=resized.current.view_revision||restored.current.input_revision!=restored.current.view_revision)return 133;
 R4DesktopActionRequest expired=r4_desktop_action_request(&token);expired.operation=5;expired.kind=R4OS_DESKTOP_ACTION_KIND_RESIZE;expired.width=800;expired.height=500;expired.deadline=r4_app_ticks(app)-1;
 R4DesktopActionReply rejected={0};if(r4_desktop_action_command(app,&expired,&rejected)||rejected.result!=R4OS_DESKTOP_ACTION_RESULT_EXPIRED)return 134;
 R4DesktopActionToken stale=token;++stale.window.owner.generation;R4DesktopActionReply wrong={0};if(r4_desktop_action_status(app,&stale,4,&wrong)||wrong.result>=0)return 135;
 R4DesktopActionReply unchanged={0};if(r4_desktop_action_status(app,&token,4,&unchanged)||unchanged.result||unchanged.current.view_revision!=restored.current.view_revision||unchanged.current.frame_generation!=restored.current.frame_generation)return 136;
 h97_text(app,"WM98 PASS same-window resize clamp minimize restore fresh-frame input-revision immutable-receipt stale and deadline\n");
 return 0;
}
#endif
