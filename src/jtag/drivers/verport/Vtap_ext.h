#ifdef __cplusplus
extern "C" {
#endif

void Vtap_start(void);
int Vtap_time_step(int tms, int tck, int trstn, int tdi);
void Vtap_finish(void);

#ifdef __cplusplus
};
#endif

