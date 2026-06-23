#include <tonc_bios.h>

void platform_app_init(void);
void platform_app_frame(void);

int main()
{
    platform_app_init();

    while (true)
    {
        VBlankIntrWait();
        platform_app_frame();
    }

    return 0;
}