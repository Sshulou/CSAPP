/***************************************************************************
 * Evil 博士的炸弹程序，版本 1.1
 * 版权所有 2011，Dr. Evil Incorporated。保留所有权利。
 *
 * 许可说明：
 *
 * Dr. Evil Incorporated（实施者）在此明确允许你（受害者）使用这个炸弹
 * 程序（以下简称“炸弹”）。这是一份限时许可，并将在受害者死亡时失效。
 * 实施者不对炸弹造成的损坏、挫败、精神失常、眼睛突出、腕管综合征、
 * 睡眠不足或其他伤害承担责任，除非实施者想把这些当作自己的功劳。
 * 受害者不得把炸弹源码分发给实施者的敌人。受害者不得通过调试、逆向
 * 工程、运行 strings、反编译、解密或其他技术了解并拆除炸弹。操作本
 * 程序时不得穿防爆服。实施者不会为自己的糟糕幽默感道歉。在法律禁止
 * 使用炸弹的地区，本许可无效。
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "support.h"
#include "phases.h"

/*
 * Evil 博士备忘：记得删除这个文件，让受害者无法知道程序如何运行，
 * 从而让他们触发一场壮观而邪恶的爆炸。
 */

FILE *infile;

int main(int argc, char *argv[])
{
    char *input;

    /* Evil 博士备忘：记得把炸弹移植到 Windows，并为它制作一个漂亮的
     * 图形界面。 */

    /* 没有提供参数时，炸弹从标准输入（通常是键盘）逐行读取答案。
     * 例如：./bomb */
    if (argc == 1) {  
	infile = stdin;
    } 

    /* 提供一个文件参数时，炸弹先从该文件逐行读取答案，读到文件末尾后
     * 再切换到标准输入。每解开一关，就可以把答案追加到文件中，从而
     * 避免下次运行时重复输入已经通过的答案。
     * 例如：./bomb answers.txt */
    else if (argc == 2) {
	if (!(infile = fopen(argv[1], "r"))) {
	    printf("%s: Error: Couldn't open %s\n", argv[0], argv[1]);
	    exit(8);
	}
    }

    /* 除程序名外，最多只能提供一个命令行参数。参数过多时打印用法并退出。 */
    else {
	printf("Usage: %s [<input_file>]\n", argv[0]);
	exit(8);
    }

    /* 初始化炸弹环境，例如安装信号处理函数和执行其他准备工作。 */
    initialize_bomb();

    printf("Welcome to my fiendish little bomb. You have 6 phases with\n");
    printf("which to blow yourself up. Have a nice day!\n");

    /* 第一关：读取一行输入，将字符串交给 phase_1 验证。 */
    input = read_line();             /* 读取本关答案                 */
    phase_1(input);                  /* 执行第一关验证               */
    phase_defused();                 /* 记录当前关卡已经成功拆除     */
    printf("Phase 1 defused. How about the next one?\n");

    /* 第二关：继续读取下一行输入并交给 phase_2 验证。 */
    input = read_line();
    phase_2(input);
    phase_defused();
    printf("That's number 2.  Keep going!\n");

    /* 第三关：通常需要分析比前两关更复杂的控制流程。 */
    input = read_line();
    phase_3(input);
    phase_defused();
    printf("Halfway there!\n");

    /* 第四关：读取答案并执行 phase_4，其中通常包含数学或递归逻辑。 */
    input = read_line();
    phase_4(input);
    phase_defused();
    printf("So you got that one.  Try this one.\n");
    
    /* 第五关：读取答案并执行 phase_5，重点关注内存访问和数据映射。 */
    input = read_line();
    phase_5(input);
    phase_defused();
    printf("Good work!  On to the next...\n");

    /* 第六关：最后一个常规关卡，通常需要分析更复杂的数据结构。 */
    input = read_line();
    phase_6(input);
    phase_defused();

    /* 六个常规关卡全部完成。注释暗示程序中可能还存在容易被忽略的内容。 */
    
    return 0;
}
