#include "combine_graphic_with_bmps_test.h"
#include "fscan_draw_gfx_using_gfx_info_test.h"

int main(){
    printf("Combine graphic with bmps test: %s\n", combine_graphic_with_bmps_test() == 0 ? "PASSED" : "FAILED");
    fscan_draw_gfx_using_gfx_info_test("GEX016.LEV");
    return 0;
}