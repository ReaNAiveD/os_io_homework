
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/


#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE* p_con);
PRIVATE void change_color(CONSOLE* p_con, u8 color, char* from, u32 len);

/*======================================================================*
			   init_screen
 *======================================================================*/
PUBLIC void init_screen(TTY* p_tty)
{
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	int v_mem_size = V_MEM_SIZE >> 1;	/* 显存总大小 (in WORD) */

	int con_v_mem_size                   = v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr      = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit        = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	for (int i = 0; i < TOTAL_LINES; i++){
		p_tty->p_console->enter[i] = SCREEN_WIDTH - 1;
	}
	p_tty->p_console->tab_count = 0;
	p_tty->p_console->color = DEFAULT_CHAR_COLOR;

	if (nr_tty == 0) {
		/* 第一个控制台沿用原来的光标位置 */
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else {
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '#');
	}

	set_cursor(p_tty->p_console->cursor);
}


/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE* p_con)
{
	return (p_con == &console_table[nr_current_console]);
}


/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE* p_con, char ch)
{
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);

	switch(ch) {
	case '\n':
		if (p_con->cursor < p_con->original_addr +
		    p_con->v_mem_limit - SCREEN_WIDTH) {
			int line = (p_con->cursor - p_con->original_addr) / SCREEN_WIDTH;
			p_con->enter[line] = (p_con->cursor - p_con->original_addr) % SCREEN_WIDTH;
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * 
				((p_con->cursor - p_con->original_addr) /
				 SCREEN_WIDTH + 1);
		}
		break;
	case '\b':
		if (p_con->cursor > p_con->original_addr) {
			if ((p_con->cursor != p_con->original_addr) && ((p_con->cursor - 
			p_con->original_addr) % SCREEN_WIDTH == 0)){
				p_con->cursor = p_con->enter[(p_con->cursor - 
				p_con->original_addr) / SCREEN_WIDTH - 1] + ((p_con->cursor - 
				p_con->original_addr) / SCREEN_WIDTH - 1) * SCREEN_WIDTH;
				p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
				*p_vmem++ = ' ';
				*p_vmem++ = DEFAULT_CHAR_COLOR;
			}
			else{
				if (p_con->tab_count > 0 && p_con->cursor == p_con->tab[p_con->tab_count - 1]){
					for (int i		 = 0; i < 4 && *(p_vmem-2) == ' '; i++){
						p_vmem -= 2;
						p_con->cursor--;
					}
					p_con->tab_count--;
				}
				else{
					p_con->cursor--;
					*(p_vmem-2) = ' ';
					*(p_vmem-1) = DEFAULT_CHAR_COLOR;
				}
			}
		}
		break;
	case '\t':
		if (p_con->cursor <
		    p_con->original_addr + p_con->v_mem_limit - 1) {
			*p_vmem++ = ' ';
			*p_vmem++ = p_con->color;
			p_con->cursor++;
			while(p_con->cursor%4 != 0){
				*p_vmem++ = ' ';
				*p_vmem++ =p_con->color;
				p_con->cursor++;
			}
			if(p_con->tab_count < MAX_TAB){
				p_con->tab[p_con->tab_count] = p_con->cursor;
				p_con->tab_count++;
			}
		}
		break;
	default:
		if (p_con->cursor <
		    p_con->original_addr + p_con->v_mem_limit - 1) {
			if ((p_con->cursor - p_con->original_addr) % SCREEN_WIDTH == SCREEN_WIDTH 
			   - 1){
				p_con->enter[(p_con->cursor - p_con->original_addr) / SCREEN_WIDTH]
				= SCREEN_WIDTH - 1;
			}
			*p_vmem++ = ch;
			*p_vmem++ = p_con->color;
			p_con->cursor++;
		}
		break;
	}

	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);
}

PUBLIC void clear_screen(CONSOLE* p_con, u32 start_addr, u32 len){
	char* true_start_addr = (char*)V_MEM_BASE + (p_con->original_addr + start_addr) * 2;
	for (char* addr = true_start_addr; addr < true_start_addr + len*2; addr+=2){
		*addr = ' ';
		*(addr+1) = DEFAULT_CHAR_COLOR;
	}
	p_con->cursor = p_con->original_addr + start_addr;
	flush(p_con);
}

PUBLIC void clear_full_screen(CONSOLE* p_con)
{
	char* start_addr = (char*)V_MEM_BASE + p_con->original_addr;
	//for (char* addr = start_addr; addr < start_addr + p_con->v_mem_limit; addr+=2){
	//	*addr = ' ';
	//	*(addr+1) = DEFAULT_CHAR_COLOR;
	//}
	//p_con->cursor = p_con->original_addr;
	//p_con->current_start_addr = p_con->original_addr;
	//flush(p_con);
	clear_screen(p_con, 0, p_con->v_mem_limit);
}

PUBLIC void reset_color(CONSOLE* p_con)
{
	char* start_addr = (char*)V_MEM_BASE + p_con->original_addr;
	for (char* addr = start_addr; addr < start_addr + p_con->v_mem_limit*2; addr+=2){
		*(addr+1) = DEFAULT_CHAR_COLOR;
	}
	flush(p_con);
}

PUBLIC void search(CONSOLE* p_con, char* str, int len)
{
	char* start_addr = V_MEM_BASE + p_con->original_addr*2;
	for (char* first = start_addr; first < start_addr + (p_con->cursor - len)*2; first+=2)
	{
		char* temp_first = first;
		int equ = 1;
		for (int i = 0; i < len; i++){
			if (*(str+i) == '\t'){
				u32 pos = (u32)(first - V_MEM_BASE) / 2 + i;
				int find = 0;
				for (int j = 0; j < p_con->tab_count; j++){
					if (p_con->tab[j] - pos <= 4 && p_con->tab[j] - pos > 0 && 
					*(first + i * 2) == ' '){
						find = 1;
						temp_first += (p_con->tab[j] - pos - 1) * 2;
						break;
					}
				}
				if (!find){
					equ = 0;
					break;
				}
			}
			else if (*(first + i * 2) != *(str+i)){
				equ = 0;
				break;
			}
		}
		if (equ){
			change_color(p_con, WHITE_BLACK, first, (temp_first-first) + len*2);
		}
	}
}

PRIVATE void change_color(CONSOLE* p_con, u8 color, char* from, u32 len)
{
	for (u32 i = 0; i < len; i+=2)
	{
		*(from+i+1) = color;
	}
	flush(p_con);
}

/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE* p_con)
{
        set_cursor(p_con->cursor);
        set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}


/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}


/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)	/* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
		return;
	}

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);
	set_video_start_addr(console_table[nr_console].current_start_addr);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE* p_con, int direction)
{
	if (direction == SCR_UP) {
		if (p_con->current_start_addr > p_con->original_addr) {
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN) {
		if (p_con->current_start_addr + SCREEN_SIZE <
		    p_con->original_addr + p_con->v_mem_limit) {
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else{
	}

	set_video_start_addr(p_con->current_start_addr);
	set_cursor(p_con->cursor);
}

