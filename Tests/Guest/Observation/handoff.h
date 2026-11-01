#ifndef DESKRUN_EVIDENCE_HANDOFF_FIXTURE_H
#define DESKRUN_EVIDENCE_HANDOFF_FIXTURE_H
#include <r4os/desktop_evidence.h>
#include <r4os/app_storage.h>

/* Public-SDK-only extension of the existing guest observation fixture.
 * Each H operation is a separate observer process. O/A deliberately discard
 * their returned token/ACK before ending; recovery uses the saved record. */
static void h97_text(R4App *app, const char *s) {
    r4sys_write_cstr(&app->system,s);
}
static void h97_number(R4App *app,uint64_t n) {
    char b[24];uint32_t i=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);
    while(i){char c[2]={b[--i],0};r4sys_write_cstr(&app->system,c);}
}
static int h97_path(const char *s,R4FilePath *out) {
    uint32_t n=0;while(s[n])++n;return r4_file_path((const uint8_t*)s,n,out)==0;
}
static int h97_load(R4App *app,const char *name,R4DesktopEvidenceToken *token) {
    R4Files files=r4_app_files(app);R4FilePath path;
    if(!h97_path(name,&path))return 0;
    R4Transfer read=r4_files_read(&files,&path,(uint8_t*)token,sizeof(*token));
    return read.state==R4_TRANSFER_BYTES&&read.bytes==sizeof(*token)&&token->version==1u&&token->size==sizeof(*token);
}
static int h97_save(R4App *app,const char *name,const R4DesktopEvidenceToken *token) {
    R4Files files=r4_app_files(app);R4FilePath path;
    if(!h97_path(name,&path))return 0;
    R4Transfer write=r4_files_write(&files,&path,(const uint8_t*)token,sizeof(*token));
    R4DesktopEvidenceToken check={0};
    if(write.state!=R4_TRANSFER_BYTES||!h97_load(app,name,&check))return 0;
    const uint8_t*a=(const uint8_t*)&check,*b=(const uint8_t*)token;
    for(uint32_t i=0;i<sizeof(check);++i)if(a[i]!=b[i])return 0;
    return 1;
}
static int32_t handoff_main(R4App *app,const uint8_t *args,uint64_t len) {
    char word[3][96]={{0}};uint32_t w=0,n=0;
    for(uint64_t i=0;i<len;++i){uint8_t c=args[i];if(c==' '||c=='\t'){if(n){if(++w==3u)return 2;n=0;}continue;}if(n>=95u)return 2;word[w][n++]=(char)c;}
    if(!word[0][0]||word[0][1])return 2;
    const char op=word[0][0];
    if(op=='P'){R4Resources resources=r4_app_resources(app);return r4_resources_program_request_desktop(&resources);}
    R4DesktopEvidenceToken token={0};R4DesktopEvidenceReply reply={0};int32_t rc;
    if(op=='C'){
        rc=r4_desktop_evidence_scope(app,&reply);
        h97_text(app,"H97 scope transport=");if(rc<0)h97_text(app,"-");h97_number(app,rc<0?(uint64_t)-(int64_t)rc:(uint32_t)rc);
        h97_text(app," result=");if(reply.result<0)h97_text(app,"-");h97_number(app,reply.result<0?(uint64_t)-(int64_t)reply.result:(uint32_t)reply.result);
        h97_text(app," broker=");h97_number(app,reply.token.broker.instance_id);h97_text(app,":");h97_number(app,reply.token.broker.generation);
        h97_text(app," epoch_nonzero=");h97_number(app,reply.token.epoch[0]|reply.token.epoch[1]|reply.token.epoch[2]);h97_text(app,"\n");
        if(rc||reply.result)return 3;
        if(!h97_save(app,word[1],&reply.token)){h97_text(app,"H97 scope file-save-readback failed\n");return 3;}
    } else {
        if(!h97_load(app,word[1],&token))return 4;
        if(op=='R')rc=r4_desktop_evidence_reserve(app,&token,&reply);
        else if(op=='O')rc=r4_desktop_evidence_offer(app,&token,&reply);
        else if(op=='A')rc=r4_desktop_evidence_acknowledge(app,&token,&reply);
        else if(op=='S')rc=r4_desktop_evidence_status(app,&token,&reply);
        else if(op=='X')rc=r4_desktop_evidence_release(app,&token,&reply);
        else if(op=='D'){
            uint8_t data[R4OS_DESKTOP_EVIDENCE_MAX_OUTPUT_CHUNK];uint32_t offset=0;uint64_t sequence=0;
            R4Files files=r4_app_files(app);R4FilePath path;if(!h97_path(word[2],&path))return 5;
            do {
                rc=r4_desktop_evidence_read(app,&token,offset,data,sizeof(data),&reply);
                if(rc||reply.result||reply.phase!=R4OS_DESKTOP_EVIDENCE_PHASE_ACKNOWLEDGED||!reply.completion.sequence||
                    (sequence&&reply.completion.sequence!=sequence)||reply.data_bytes>sizeof(data))return 6;
                sequence=reply.completion.sequence;
                R4Transfer write=offset?r4_files_append(&files,&path,data,reply.data_bytes):r4_files_write(&files,&path,data,reply.data_bytes);
                if(write.state!=R4_TRANSFER_BYTES)return 7;
                offset+=reply.data_bytes;
                if(!reply.data_bytes&&offset<reply.completion.output_length)return 8;
            }while(offset<reply.completion.output_length);
            R4ProgramInstanceInfo info={0};
            const int32_t live=((R4SysProgramHandleStatusFn)(uintptr_t)app->system.table->program_handle_status)(&token.process,&info);
            h97_text(app,"H97 target_live_status=");h97_number(app,(uint32_t)live);h97_text(app," bytes_saved=");h97_number(app,offset);h97_text(app,"\n");
        } else return 2;
        if(rc)return 9;
        if((op=='R'||op=='S')&&word[2][0]&&word[2][0]!='-'&&!h97_save(app,word[2],&reply.token))return 10;
    }
    h97_text(app,"H97 op=");char operation[2]={op,0};h97_text(app,operation);
    h97_text(app," result=");if(reply.result<0)h97_text(app,"-");h97_number(app,reply.result<0?(uint64_t)-(int64_t)reply.result:(uint32_t)reply.result);
    h97_text(app," phase=");h97_number(app,reply.phase);h97_text(app," serial=");h97_number(app,reply.token.serial);
    h97_text(app," process=");h97_number(app,reply.token.process.instance_id);h97_text(app,":");h97_number(app,reply.token.process.generation);
    h97_text(app," sequence=");h97_number(app,reply.completion.sequence);h97_text(app," exit=");if(reply.completion.exit_code<0)h97_text(app,"-");h97_number(app,reply.completion.exit_code<0?(uint64_t)-(int64_t)reply.completion.exit_code:(uint32_t)reply.completion.exit_code);
    h97_text(app," length=");h97_number(app,reply.completion.output_length);h97_text(app," flags=");h97_number(app,reply.completion.flags);h97_text(app,"\n");
    return 0;
}
#endif
