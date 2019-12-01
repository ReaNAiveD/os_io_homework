
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               tty.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

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

#define TTY_FIRST	(tty_table)
#define TTY_END		(tty_table + NR_CONSOLES)

PRIVATE void init_tty(TTY* p_tty);
PRIVATE void tty_do_read(TTY* p_tty);
PRIVATE void tty_do_write(TTY* p_tty);
PRIVATE void put_key(TTY* p_tty, u32 key);
PRIVATE void switch_mode(TTY* p_tty, TTY_MODE mode);

/*======================================================================*
                           task_tty
 *======================================================================*/
PUBLIC void task_tty()
{
	TTY*	p_tty;

	init_keyboard();

	for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
		init_tty(p_tty);
	}
	select_console(0);
	int temp = 0;
	int last_clear_tick = 0;
	while (1) {
		for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
			tty_do_read(p_tty);
			tty_do_write(p_tty);
		}
		if ((temp++)%1000 == 0){
			int current_tick = sys_get_ticks();
			if (current_tick - last_clear_tick > 20000){
				for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
				if (is_current_console(p_tty->p_console) && 
				    p_tty->mode == EDIT_MODE){
					clear_full_screen(p_tty->p_console);
				}
				}
				last_clear_tick = current_tick;
			}
		}
	}
}

/*======================================================================*
			   init_tty
 *======================================================================*/
PRIVATE void init_tty(TTY* p_tty)
{
	p_tty->inbuf_count = 0;
	p_tty->p_inbuf_head = p_tty->p_inbuf_tail = p_tty->in_buf;

	p_tty->mode = EDIT_MODE;
	p_tty->search_count = 0;

	init_screen(p_tty);
	clear_full_screen(p_tty->p_console);
}

/*======================================================================*
				in_process
 *======================================================================*/
PUBLIC void in_process(TTY* p_tty, u32 key)
{
        char output[2] = {'\0', '\0'};

	if (p_tty->mode == EDIT_MODE){
		if (!(key & FLAG_EXT)) {
			put_key(p_tty, key);
		}
		else {
		        int raw_code = key & MASK_RAW;
		        switch(raw_code) {
		        case ENTER:
				put_key(p_tty, '\n');
				break;
		        case BACKSPACE:
				put_key(p_tty, '\b');
				break;
		        case UP:
		                if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
					scroll_screen(p_tty->p_console, SCR_DN);
		                }
				break;
			case DOWN:
				if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
					scroll_screen(p_tty->p_console, SCR_UP);
				}
				break;
			case TAB:
				put_key(p_tty, '\t');
				break;
			case F1:
			case F2:
			case F3:
			case F4:
			case F5:
			case F6:
			case F7:
			case F8:
			case F9:
			case F10:
			case F11:
			case F12:
				/* Alt + F1~F12 */
				//if ((key & FLAG_ALT_L) || (key & FLAG_ALT_R)) {
					select_console(raw_code - F1);
				//}
				break;
			case ESC:
				switch_mode(p_tty, SEARCH_MODE);
				break;
		        default:
		                break;
		        }
		}
	}
	else if (p_tty->mode == SEARCH_MODE)
	{
		if (!(key & FLAG_EXT)) {
			put_key(p_tty, key);
			p_tty->search_buf[p_tty->search_count] = (char)key;
			p_tty->search_count++;
		}
		else {
		        int raw_code = key & MASK_RAW;
		        switch(raw_code) {
		        case ENTER:
				switch_mode(p_tty, SEARCH_MODE_END);
				break;
		        case BACKSPACE:
				put_key(p_tty, '\b');
				if (p_tty->search_count > 0) p_tty->search_count--;
				break;
		        case UP:
		                if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
					scroll_screen(p_tty->p_console, SCR_DN);
		                }
				break;
			case DOWN:
				if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
					scroll_screen(p_tty->p_console, SCR_UP);
				}
				break;
			case TAB:
				put_key(p_tty, '\t');
				p_tty->search_buf[p_tty->search_count] = '\t';
				p_tty->search_count++;
				break;
		        default:
		                break;
		        }
		}
	}
	else if (p_tty->mode == SEARCH_MODE_END)
	{
		int raw_code = key & MASK_RAW;
		if (raw_code == ESC){
			switch_mode(p_tty, EDIT_MODE);
		}
	}
}

PRIVATE void switch_mode(TTY* p_tty, TTY_MODE mode){
	p_tty->mode = mode;
	switch(mode){
	case EDIT_MODE:
		for (int i = 0; i < p_tty->search_count; i++){
			if (p_tty->search_buf[i] == '\t'){
				p_tty->p_console->tab_count--;
			}
		}
		p_tty->search_count=0;
		clear_screen(p_tty->p_console, p_tty->edit_end_pos, p_tty->p_console->cursor - p_tty->edit_end_pos);
		p_tty->p_console->color = DEFAULT_CHAR_COLOR;
		p_tty->p_console->cursor = p_tty->edit_end_pos;
		reset_color(p_tty->p_console);
		break;
	case SEARCH_MODE:
		p_tty->p_console->color = WHITE_BLACK;
		p_tty->edit_end_pos = p_tty->p_console->cursor;
		break;
	case SEARCH_MODE_END:
		search(p_tty->p_console, p_tty->search_buf, p_tty->search_count);
		break;
	}
}

/*======================================================================*
			      put_key
*======================================================================*/
PRIVATE void put_key(TTY* p_tty, u32 key)
{
	if (p_tty->inbuf_count < TTY_IN_BYTES) {
		*(p_tty->p_inbuf_head) = key;
		p_tty->p_inbuf_head++;
		if (p_tty->p_inbuf_head == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_head = p_tty->in_buf;
		}
		p_tty->inbuf_count++;
	}
}


/*======================================================================*
			      tty_do_read
 *======================================================================*/
PRIVATE void tty_do_read(TTY* p_tty)
{
	if (is_current_console(p_tty->p_console)) {
		keyboard_read(p_tty);
	}
}


/*======================================================================*
			      tty_do_write
 *======================================================================*/
PRIVATE void tty_do_write(TTY* p_tty)
{
	if (p_tty->inbuf_count) {
		char ch = *(p_tty->p_inbuf_tail);
		p_tty->p_inbuf_tail++;
		if (p_tty->p_inbuf_tail == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_tail = p_tty->in_buf;
		}
		p_tty->inbuf_count--;

		out_char(p_tty->p_console, ch);
	}
}


