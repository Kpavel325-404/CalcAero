#define CA_NO_PRINTF_WRAP
#include "app.h"
#include "l10n.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void put(const char *dir,const char *text){char p[300];snprintf(p,sizeof(p),"%s/config.json",dir);FILE *f=fopen(p,"wb");assert(f);fputs(text,f);assert(!fclose(f));}
int main(void){
 char tmp[]="build/config-XXXXXX";char *dir=mkdtemp(tmp);assert(dir);int lang=-1;
 assert(ca_language()==LANG_EN && LANG_COUNT==26);
 assert(!ca_config_load(dir,&lang)&&lang==LANG_EN);
 const char *invalid[]={"","{}","{\"language\":\"\"}","{\"language\":null}","{\"language\":\"xx\"}","{\"language\":\"ru\",\"language\":\"de\"}","{\"language\":\"ru\"} trailing","{\"language\":\"ru\"","[\"ru\"]","{\"language\":123}","{\"language\":\"../../ru\"}"};
 for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){put(dir,invalid[i]);lang=-1;assert(!ca_config_load(dir,&lang)&&lang==LANG_EN);}
 for(int i=0;i<LANG_COUNT;i++){
  assert(ca_language_from_code(ca_language_code(i))==i);
  assert(ca_config_save(dir,i));assert(ca_config_load(dir,&lang)&&lang==i);
  assert(ca_locale_load(".",i)&&ca_language()==i);
  assert(ca_format_compatible("%llu %.15g %s","%llu %.15g %s"));
  assert(!ca_format_compatible("%d %s","%s %d"));
  CLApp app;cl_app_init(&app);cl_app_page(&app,PAGE_LANGUAGE);
  app.menu=1;cl_app_key(&app,KEY_SELECT);assert(app.page==PAGE_LANGUAGE && !app.menu);
  app.language_row=i;cl_app_key(&app,KEY_CROSS);assert(app.request_language==i);
  app.language_row=0;cl_app_key(&app,KEY_UP);assert(app.language_row==25);
  cl_app_key(&app,KEY_DOWN|KEY_RIGHT);assert(app.language_row==13);
 }
 // Write failure keeps the previous readable language unchanged.
 assert(ca_config_save(dir,LANG_RU));char temp[300];snprintf(temp,sizeof(temp),"%s/config.json.tmp",dir);assert(!mkdir(temp,0700));
 assert(!ca_config_save(dir,LANG_DE));assert(ca_config_load(dir,&lang)&&lang==LANG_RU);assert(!rmdir(temp));
 // Missing catalogs cannot replace the current working language.
 assert(ca_locale_load(".",LANG_RU));assert(!ca_locale_load(dir,LANG_DE));assert(ca_language()==LANG_RU);
 unsigned r=20261009;
 for(int i=0;i<5000;i++){
  char text[300];int n=i%299;for(int j=0;j<n;j++){r=r*1664525u+1013904223u;text[j]=(char)(32+r%95);}text[n]=0;put(dir,text);
  lang=-1;int ok=ca_config_load(dir,&lang);assert(ok?(lang>=0&&lang<26):lang==LANG_EN);
 }
 ca_locale_close();assert(ca_language()==LANG_EN);
 puts("PASS: 26 language catalogs, immediate JSON saves, default English, invalid/unset config, I/O rollback, 5000 malformed configs, controller selection");
 return 0;
}
