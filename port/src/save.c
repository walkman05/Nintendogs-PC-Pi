/* Save memory (full-game mode). The demo has no backup-memory code: where the retail game reads and writes its
 * save chip (the backup layout table at data_0212f5c0: Registory, Dog, SoundCol, Voice, HotelDog, HotelSC, Friend;
 * each block twice, the second copy 0x20000 higher), the demo's func_02096880 and func_02097608 call these
 * instead. The chip's contents live in a file, <data>/save/nintendogs.sav, at the chip addresses. */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static char save_dir[400], save_path[440];

void ndogs_save_set_dir(const char *data_dir)
{
    snprintf(save_dir, sizeof save_dir, "%s/save", data_dir);
    snprintf(save_path, sizeof save_path, "%s/nintendogs.sav", save_dir);
}

/* Returns 1 when a save file exists (bytes past its end read as 0xff, like erased flash), 0 when there is none
 * (the caller then leaves its buffer alone, as the demo does). */
int ndogs_save_read(unsigned addr, void *dst, unsigned len)
{
    FILE *f = fopen(save_path, "rb");
    size_t n = 0;
    if (!f)
        return 0;
    memset(dst, 0xff, len);
    if (fseek(f, (long)addr, SEEK_SET) == 0)
        n = fread(dst, 1, len, f);
    (void)n;
    fclose(f);
    return 1;
}

void ndogs_save_write(unsigned addr, const void *src, unsigned len)
{
    FILE *f = fopen(save_path, "r+b");
    long size;
    if (!f) {
#ifdef _WIN32
        _mkdir(save_dir);
#else
        mkdir(save_dir, 0777);
#endif
        f = fopen(save_path, "w+b");
        if (!f) {
            fprintf(stderr, "save: cannot write %s\n", save_path);
            return;
        }
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    while (size < (long)addr) {  /* unwritten chip space reads as erased */
        fputc(0xff, f);
        size++;
    }
    fseek(f, (long)addr, SEEK_SET);
    fwrite(src, 1, len, f);
    fclose(f);
}
