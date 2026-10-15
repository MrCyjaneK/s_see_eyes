#ifndef SPACESEE_EYES_H
#define SPACESEE_EYES_H

#ifdef __cplusplus
extern "C" {
#endif

int SPACESEE_eyes_load(const char *workDir, const char *assetsDir);
const char *SPACESEE_eyes_loadError(void);
void SPACESEE_eyes_unload(void);
int SPACESEE_eyes_isHeavy(void);
int SPACESEE_eyes_assetCount(void);
const char *SPACESEE_eyes_asset(int index);
const char *SPACESEE_eyes_version(void);
const char *SPACESEE_eyes_extension(void);
const char *SPACESEE_eyes_filetypes(void);
long long SPACESEE_eyes_maxInputSize(void);
long long SPACESEE_eyes_analyze(const char *inputPath, const char *outputPath, const char *dataDir);
int SPACESEE_eyes_progress(long long jobId, int scale);
const char *SPACESEE_eyes_progressString(long long jobId);
void SPACESEE_eyes_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
