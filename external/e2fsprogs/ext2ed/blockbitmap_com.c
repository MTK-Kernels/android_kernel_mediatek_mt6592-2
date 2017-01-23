/*

/usr/src/ext2ed/blockbitmap_com.c

A part of the extended file system 2 disk editor.

-------------------------
Handles the block bitmap.
-------------------------

This file implements the commands which are specific to the blockbitmap type.

First written on: July 5 1995

Copyright (C) 1995 Gadi Oxman

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext2ed.h"


void type_ext2_block_bitmap___entry (char *command_line)


{
	unsigned long entry_num;
	char *ptr,buffer [80];



	ptr=parse_word (command_line,buffer);					
	if (*ptr==0) {
		wprintw (command_win,"Error - No argument specified\n");
		refresh_command_win ();	return;
	}
	ptr=parse_word (ptr,buffer);

	entry_num=atol (buffer);


	if (entry_num >= file_system_info.super_block.s_blocks_per_group) {	

		wprintw (command_win,"Error - Entry number out of bounds\n");
		refresh_command_win ();return;
	}



	block_bitmap_info.entry_num=entry_num;					
	strcpy (buffer,"show");dispatch (buffer);				
}

void type_ext2_block_bitmap___next (char *command_line)


{
	long entry_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		entry_offset=atol (buffer);
	}

	sprintf (buffer,"entry %ld",block_bitmap_info.entry_num+entry_offset);
	dispatch (buffer);
}

void type_ext2_block_bitmap___prev (char *command_line)

{
	long entry_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		entry_offset=atol (buffer);
	}

	sprintf (buffer,"entry %ld",block_bitmap_info.entry_num-entry_offset);
	dispatch (buffer);
}

void type_ext2_block_bitmap___allocate (char *command_line)


{
	long entry_num,num=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);					
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		num=atol (buffer);
	}

	entry_num=block_bitmap_info.entry_num;
										
	if (num > file_system_info.super_block.s_blocks_per_group-entry_num) {
		wprintw (command_win,"Error - There aren't that much blocks in the group\n");
		refresh_command_win ();return;
	}

	while (num) {								
		allocate_block (entry_num);					
		num--;entry_num++;
	}

	dispatch ("show");							
}

void type_ext2_block_bitmap___deallocate (char *command_line)


{
	long entry_num,num=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		num=atol (buffer);
	}

	entry_num=block_bitmap_info.entry_num;
	if (num > file_system_info.super_block.s_blocks_per_group-entry_num) {
		wprintw (command_win,"Error - There aren't that much blocks in the group\n");
		refresh_command_win ();return;
	}

	while (num) {
		deallocate_block (entry_num);
		num--;entry_num++;
	}

	dispatch ("show");
}


void allocate_block (long entry_num)


{
	unsigned char bit_mask=1;
	int byte_offset,j;

	byte_offset=entry_num/8;					
									
	for (j=0;j<entry_num%8;j++)
		bit_mask*=2;						
	type_data.u.buffer [byte_offset] |= bit_mask;			
}

void deallocate_block (long entry_num)


{
	unsigned char bit_mask=1;
	int byte_offset,j;

	byte_offset=entry_num/8;
	for (j=0;j<entry_num%8;j++)
		bit_mask*=2;
	bit_mask^=0xff;

	type_data.u.buffer [byte_offset] &= bit_mask;
}

void type_ext2_block_bitmap___show (char *command_line)


{
	int i,j;
	unsigned char *ptr;
	unsigned long block_num,entry_num;

	ptr=type_data.u.buffer;
	show_pad_info.line=0;show_pad_info.max_line=-1;

	wmove (show_pad,0,0);
	for (i=0,entry_num=0;i<file_system_info.super_block.s_blocks_per_group/8;i++,ptr++) {
		for (j=1;j<=128;j*=2) {						
			if (entry_num==block_bitmap_info.entry_num) {		
				wattrset (show_pad,A_REVERSE);
				show_pad_info.line=show_pad_info.max_line-show_pad_info.display_lines/2;
			}

			if ((*ptr) & j)						
				wprintw (show_pad,"1");
			else
				wprintw (show_pad,"0");

			if (entry_num==block_bitmap_info.entry_num)
				wattrset (show_pad,A_NORMAL);

			entry_num++;						
		}
		wprintw (show_pad," ");
		if (i%8==7) {							
			wprintw (show_pad,"\n");
			show_pad_info.max_line++;
		}
	}

	refresh_show_pad ();
	show_info ();								

										
	wmove (show_win,1,0);
	wprintw (show_win,"Block bitmap of block group %ld\n",block_bitmap_info.group_num);
										

	block_num=block_bitmap_info.entry_num+block_bitmap_info.group_num*file_system_info.super_block.s_blocks_per_group;
	block_num+=file_system_info.super_block.s_first_data_block;

	wprintw (show_win,"Status of block %ld - ",block_num);			
	ptr=type_data.u.buffer+block_bitmap_info.entry_num/8;
	j=1;
	for (i=block_bitmap_info.entry_num % 8;i>0;i--)
		j*=2;
	if ((*ptr) & j)
		wprintw (show_win,"Allocated\n");
	else
		wprintw (show_win,"Free\n");
	refresh_show_win ();
}
