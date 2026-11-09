/* Written by tests/stub/update-profiler-stub from the headers of TAU:
   the TAU macros of config_files/tau_config.yaml and the declarations
   that they use. Do not edit. */
#ifndef SALT_PROFILER_STUB_H
#define SALT_PROFILER_STUB_H
#include <stddef.h>
#define TAU_VERSION "2.35.2-git"
typedef unsigned long TauGroup_t;
#define TAU_DEFAULT 		0xffffffff   /* All profiling groups enabled*/
#define TAU_USER    		0x80000000   /* User 	      'u' */
void Tau_profile_c_timer(void **ptr, const char *fname, const char *type, TauGroup_t group, const char *group_name);
void Tau_lite_start_timer(void *profiler, int phase);
void Tau_lite_stop_timer(void *profiler);
void Tau_init_ref(int* argc, char ***argv);
void Tau_set_node(int node);
#define TAU_PROFILE_TIMER(var,name, type, group) static void *var=NULL; Tau_profile_c_timer(&var, name, type, group, #group);
#define TAU_PROFILE_START(var) Tau_lite_start_timer(var, 0);
#define TAU_PROFILE_STOP(var) Tau_lite_stop_timer(var);
#define TAU_INIT(argc, argv)			Tau_init_ref(argc, argv);
#define TAU_PROFILE_SET_NODE(node) 		Tau_set_node(node);
#ifdef __cplusplus
void tauCreateFI(void **ptr, const char *name, const char *type,
		 TauGroup_t ProfileGroup , const char *ProfileGroupName);
struct Tau_Profile_Wrapper
{
  void * fInfo;

  Tau_Profile_Wrapper(void * fi, int phase = 0) : fInfo(fi) {
#ifndef TAU_PROFILEPHASE
    phase = 0;
#endif
    if (fi) {
      Tau_lite_start_timer(fi, phase);
    }
  }

  ~Tau_Profile_Wrapper() {
    if (fInfo) {
      Tau_lite_stop_timer(fInfo);
    }
  }
};
#define TAU_PROFILE(name, type, group) \
  static void *tauFI = 0; \
  if (tauFI == 0) tauCreateFI(&tauFI, name, type, (TauGroup_t)group, #group); \
  Tau_Profile_Wrapper tauFProf(tauFI);
#endif /* __cplusplus */
#endif /* SALT_PROFILER_STUB_H */
