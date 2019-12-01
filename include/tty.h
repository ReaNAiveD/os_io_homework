
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				tty.h
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#ifndef _ORANGES_TTY_H_
#define _ORANGES_TTY_H_


#define TTY_IN_BYTES	256	/* tty input queue size */
#define SEARCH_SIZE	256

typedef int TTY_MODE;

struct s_console;

/* TTY */
typedef struct s_tty
{
	u32	in_buf[TTY_IN_BYTES];	/* TTY 输入缓冲区 */
	u32*	p_inbuf_head;		/* 指向缓冲区中下一个空闲位置 */
	u32*	p_inbuf_tail;		/* 指向键盘任务应处理的键值 */
	int	inbuf_count;		/* 缓冲区中已经填充了多少 */

	struct s_console *	p_console;

	TTY_MODE	mode;

	char	search_buf[SEARCH_SIZE];

	int	search_count;

	unsigned int	edit_end_pos;	/* huancun editmode mowei dizhi */
}TTY;

#define EDIT_MODE	0
#define SEARCH_MODE	1
#define SEARCH_MODE_END	2


#endif /* _ORANGES_TTY_H_ */
