#include "gui_use.h"

int block_use(dxf_drawing *drawing);
int block_rename(dxf_drawing *drawing, char *curr_name, char *new_name);
int block_select(gui_obj *gui, char *name);

int gui_block_interactive(gui_obj *gui){
	if (gui->modal == NEW_BLK){
		if (gui->step == 0) {
			/* try to go to next step */
			gui->step = 1;
			gui->free_sel = 0;
		}
		/* verify if elements in selection list */
		if (gui->step == 1 && (!gui->sel_list->next || (gui->ev & EV_ADD))){
			/* if selection list is empty, back to first step */
			gui->step = 0;
			gui->free_sel = 1;
		}
		
		if (gui->step == 0){
			/* in first step, select the elements to proccess*/
			gui->en_distance = 0;
			gui->sel_ent_filter = ~DXF_NONE;
			gui_simple_select(gui);
      /* user cancel operation */
      if (gui->ev & EV_CANCEL){
        gui->element = NULL;
        gui_default_modal(gui);
        gui->step = 0;
      }
		}
		else if (gui->step == 1){
			/* Enter insert point */
			if (gui->ev & EV_ENTER){
				gui->en_distance = 1;
				gui->step_x[gui->step + 1] = gui->step_x[gui->step];
				gui->step_y[gui->step + 1] = gui->step_y[gui->step];
				gui->step = 2;
				gui->step_x[gui->step + 1] = gui->step_x[gui->step];
				gui->step_y[gui->step + 1] = gui->step_y[gui->step];
				gui_next_step(gui);
			}
			else if (gui->ev & EV_CANCEL){
				gui_default_modal(gui);
			}
		}
		else{
			if (gui->ev & EV_ENTER){
				/* confirm block creation */
				dxf_node *blkrec = NULL, *blk = NULL;
				if (dxf_new_block (gui->drawing, gui->blk_name, gui->blk_descr,
					(double []){gui->step_x[1], gui->step_y[1], 0.0},
					gui->text2tag, gui->tag_mark, gui->hide_mark, gui->value_mark, gui->dflt_value,
					"0", gui->sel_list, &blkrec, &blk, DWG_LIFE))
				{
					do_add_entry(&gui->list_do, _l("NEW BLOCK")); /* undo/redo list*/
					do_add_item(gui->list_do.current, NULL, blkrec); /* undo/redo list*/
					do_add_item(gui->list_do.current, NULL, blk); /* undo/redo list*/
				}
				gui_default_modal(gui);
			}
			else if (gui->ev & EV_CANCEL){
				//gui_default_modal(gui);
				gui->step = 1;
			}
		}
	}
	
	return 1;
}

int gui_block_info (gui_obj *gui){
	if (gui->modal == NEW_BLK) {
		nk_layout_row_dynamic(gui->ctx, 20, 1);
		nk_label(gui->ctx, _l("Create a new block"), NK_TEXT_LEFT);
		nk_label_colored(gui->ctx, gui->blk_name, NK_TEXT_CENTERED, nk_rgb(255,255,0));
		if (gui->step == 0){
			nk_label(gui->ctx, _l("Select/Add element"), NK_TEXT_LEFT);
		}
		else if (gui->step == 1){
			nk_label(gui->ctx, _l("Enter insert point"), NK_TEXT_LEFT);
		} else {
			nk_label(gui->ctx, _l("Confirm"), NK_TEXT_LEFT);
		}
		
	}
	return 1;
}

/* block manager window */
int gui_blk_mng (gui_obj *gui){
	int i, j, show_blk_mng = 1;
	
	static int show_hidden_blks = 0, show_blk_edit = 0, show_attr_edit = 0;
	static int create = 0, show_blk_create = 0, xref = 0, xref_full = 0;
	static char txt[DXF_MAX_CHARS+1] = "";
	static char descr[DXF_MAX_CHARS+1] = "";
	static char new_name[DXF_MAX_CHARS+1] = "";
	static char new_descr[DXF_MAX_CHARS+1] = "";
	static char path[DXF_MAX_CHARS+1] = "";
	static int blk_idx = -1;
	static int show_app_file = 0;
	
	static enum From {INTERACTIVE, FROM_FILE} from = INTERACTIVE;
	
	gui->next_win_x += gui->next_win_w + 3;
	//gui->next_win_y += gui->next_win_h + 3;
	gui->next_win_w = 650;
	gui->next_win_h = 400;
	
	if (nk_begin(gui->ctx, _l("Blocks Manager"), nk_rect(gui->next_win_x, gui->next_win_y, gui->next_win_w, gui->next_win_h),
	NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
	NK_WINDOW_CLOSABLE|NK_WINDOW_TITLE)){
		struct nk_style_button *sel_type;
		
		list_node *blk_g; /*graphic object of current block */
		dxf_node *blk, *blk_nm, *next; /* current block and its name attribute */
		int blk_ei; /*extents flag of current block */
		/* extents and zoom parameters */
		double blk_x0, blk_y0, blk_x1, blk_y1, z, z_x, z_y, o_x, o_y;
		double blk_z0, blk_z1;
    
    char *name = NULL;
		
		block_use(gui->drawing); /* update blocks in use*/
		
		/* dynamic width for block list and fixed width for preview image */
		nk_layout_row_template_begin(gui->ctx, 280);
		nk_layout_row_template_push_dynamic(gui->ctx);
		nk_layout_row_template_push_static(gui->ctx, 190);
		nk_layout_row_template_end(gui->ctx);
		i = 0;
		
		/*  show block list */
		if (nk_group_begin(gui->ctx, "Block_list", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
			/* header */
			nk_layout_row_dynamic(gui->ctx, 32, 1);
			if (nk_group_begin(gui->ctx, "Block_head", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
				/* dynamic width for block name and fixed width for use mark */
				nk_layout_row_template_begin(gui->ctx, 22);
				nk_layout_row_template_push_dynamic(gui->ctx);
				nk_layout_row_template_push_static(gui->ctx, 50);
				nk_layout_row_template_push_static(gui->ctx, 50);
				nk_layout_row_template_push_static(gui->ctx, 8);
				nk_layout_row_template_end(gui->ctx);
				
				if (nk_button_label(gui->ctx, _l("Name"))){
					
				}
				if (nk_button_label(gui->ctx, _l("Attr"))){
					
				}
				if (nk_button_label(gui->ctx, _l("Used"))){
					
				}
				nk_group_end(gui->ctx);
			}
			/* list */
			nk_layout_row_dynamic(gui->ctx, 225, 1);
			if (nk_group_begin(gui->ctx, "Block_names", NK_WINDOW_BORDER)) {
				nk_layout_row_template_begin(gui->ctx, 20);
				nk_layout_row_template_push_dynamic(gui->ctx);
				nk_layout_row_template_push_static(gui->ctx, 50);
				nk_layout_row_template_push_static(gui->ctx, 50);
				nk_layout_row_template_end(gui->ctx);
				
				i = 0; next = NULL;
				blk = dxf_find_obj_nxt(gui->drawing->blks, &next, "BLOCK");
				while (blk){
					/* get name of current block */
					blk_nm = dxf_find_attr2(blk, 2);
          name = NULL;
          if (blk_nm) name = (char*) strpool_cstr( &name_pool, blk_nm->value.str);
					if (name){
						/* show block name, according hidden option */
						if((name[0] != '*') || (show_hidden_blks)){ /* hidden blocks name starts with '*' */
							
							/* verify if curren block is selected */
							sel_type = &gui->b_icon_unsel;
							if (strcmp(gui->blk_name, name) == 0) sel_type = &gui->b_icon_sel;
							/* block name */
							if (nk_button_label_styled(gui->ctx, sel_type, name)){
								/* select current block */
								blk_idx = i;
								strncpy(gui->blk_name, name, DXF_MAX_CHARS);
							}
							
							/* Attributes definition in block */
							char num_attr[10];
							j = 0;
							while (dxf_find_obj_i(blk, "ATTDEF", j)){
								j++;
							}
							if (j == 0) snprintf(num_attr, 9, "-");
							else snprintf(num_attr, 9, "%d", j);
							nk_label(gui->ctx, num_attr, NK_TEXT_CENTERED);
							
							/* verify if block is used in drawing, by count in layer index*/
							if (blk->obj.layer > 0)
								nk_label(gui->ctx, "x", NK_TEXT_CENTERED);
							else nk_label(gui->ctx, " ", NK_TEXT_CENTERED);
						}
					}
					
					i++;
					if (next)
						blk = dxf_find_obj_nxt(gui->drawing->blks, &next, "BLOCK");
					else
						blk = NULL;
				}
				nk_group_end(gui->ctx);
			}
			nk_group_end(gui->ctx);
		}
		/* update the selected block information and its preview image */
		if (blk_idx >= 0){
			blk = dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", gui->blk_name);
			blk_idx = -1;
			
			/* get description of current block */
			blk_nm = dxf_find_attr2(blk, 4);
			if (blk_nm){
				strncpy(descr, //blk_nm->value.s_data, DXF_MAX_CHARS);
          strpool_cstr( &value_pool, blk_nm->value.str), DXF_MAX_CHARS);
			}
			
			blk_ei = 0;
			/* get graphics of current block*/
			blk_g = dxf_graph_parse(gui->drawing, blk, 0, FRAME_LIFE);
			/* get extents parameters of current block*/
			graph_list_ext(blk_g, &blk_ei, &blk_x0, &blk_y0, &blk_z0, &blk_x1, &blk_y1, &blk_z1);
			
			/* calcule the zoom and offset for preview */
			z_x = fabs(blk_x1 - blk_x0)/gui->preview[PRV_BLOCK]->width;
			z_y = fabs(blk_y1 - blk_y0)/gui->preview[PRV_BLOCK]->height;
			z = (z_x > z_y) ? z_x : z_y;
			if (z <= 0) z =1;
			else z = 1/(1.1 * z);
			o_x = blk_x0 - (fabs((blk_x1 - blk_x0)*z - gui->preview[PRV_BLOCK]->width)/2)/z;
			o_y = blk_y0 - (fabs((blk_y1 - blk_y0)*z - gui->preview[PRV_BLOCK]->height)/2)/z;
			
			/* show block extention */
			snprintf(txt, DXF_MAX_CHARS, "(%0.2f,%0.2f)-(%0.2f,%0.2f)", blk_x0, blk_y0, blk_x1, blk_y1);
			
			/* draw graphics in current preview bitmap */
			bmp_fill(gui->preview[PRV_BLOCK], gui->preview[PRV_BLOCK]->bkg); /* clear bitmap */
			struct draw_param d_param;
			d_param.ofs_x = o_x;
			d_param.ofs_y = o_y;
			d_param.ofs_z = 0;
			d_param.scale = z;
			d_param.list = NULL;
			d_param.subst = NULL;
			d_param.len_subst = 0;
			d_param.inc_thick = 0;
			graph_list_draw(blk_g, gui->preview[PRV_BLOCK], d_param);
			
		}
		/* Selected block detailed information and preview image */
		if (nk_group_begin(gui->ctx, "Block_prev", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
			/* selected block name */
			nk_layout_row_dynamic(gui->ctx, 20, 1);
			nk_label_colored(gui->ctx, gui->blk_name, NK_TEXT_CENTERED, nk_rgb(255,255,0));
			
			/* preview img */
			nk_layout_row_dynamic(gui->ctx, 175, 1);
			nk_button_image(gui->ctx,  nk_image_ptr(gui->preview[PRV_BLOCK]));
			
			/* current block corners */
			nk_layout_row_dynamic(gui->ctx, 15, 1);
			nk_style_push_font(gui->ctx, &(gui->alt_font_sizes[FONT_SMALL])); /* change font to tiny*/
			nk_label(gui->ctx, txt, NK_TEXT_CENTERED);
			nk_style_pop_font(gui->ctx); /* back to default font*/
			
			/* description */
			nk_layout_row_dynamic(gui->ctx, 50, 1);
			nk_label_colored_wrap(gui->ctx, descr, nk_rgb(100,115,255));
			
			nk_group_end(gui->ctx);
		}
		
		/* option to show hidden blocks */
		nk_layout_row_dynamic(gui->ctx, 20, 1);
		nk_checkbox_label(gui->ctx, _l("Hidden"), &show_hidden_blks);
		
		nk_layout_row_dynamic(gui->ctx, 20, 5);
		
		/* create new block */
		if (nk_button_label(gui->ctx, _l("Create"))){
			/* show create popup */
			show_blk_create = 1;
			new_name[0] = 0;
			gui->blk_descr[0] = 0;
		}
		
		/* edit selected block */
		if (nk_button_label(gui->ctx, _l("Edit"))){
			/* verify if block point by name exists in structure */
			if(dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", gui->blk_name)){
				/* show edit popup */
				show_blk_edit = 1;
				new_name[0] = 0;
				strncpy(new_name, gui->blk_name, DXF_MAX_CHARS);
				strncpy(new_descr, descr, DXF_MAX_CHARS);
			}
		}
		
		/* delete selected Block */
		if (nk_button_label(gui->ctx, _l("Remove"))){
			block_use(gui->drawing); /* update blocks in use for sure*/
			
			/*get block object by selected name */
			blk = dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", gui->blk_name);
			if(blk) {
				/* don't remove block in use */
				if (blk->obj.layer){ /* uses block's layer index to count */
					snprintf(gui->log_msg, 63, _l("Error: Don't remove Block in use"));
				}
				else{
					do_add_entry(&gui->list_do, _l("Remove Block"));
					/* remove block from main structure */
					do_add_item(gui->list_do.current, blk, NULL);
					dxf_obj_subst(blk, NULL);
					
					/*get BLOCK_RECORD object by selected name */
					blk = dxf_find_obj_descr2(gui->drawing->blks_rec, "BLOCK_RECORD", gui->blk_name);
					if(blk) {
						/* remove block record from main structure */
						do_add_item(gui->list_do.current, blk, NULL);
						dxf_obj_subst(blk, NULL);
					}
				}
			}
		}
		/* edit attrib_def in Block */
		if (nk_button_label(gui->ctx, _l("Attributes"))){
			/* verify if block point by name exists in structure */
			if(blk = dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", gui->blk_name)){
				if (dxf_find_obj2(blk, "ATTDEF"))
					/* show attdef edit popup */
					show_attr_edit = 1;
			}
			
		}
		/* select block relative entities in drawing*/
		if (nk_button_label(gui->ctx, _l("Select"))){
			gui->modal = SELECT;
			block_select(gui, gui->blk_name);
		}
		
		if ((show_blk_edit)){ /* block edit popup interface */
			if (nk_popup_begin(gui->ctx, NK_POPUP_STATIC, _l("Edit Block"), NK_WINDOW_CLOSABLE, nk_rect(200, 40, 250, 220))){
				
				/* enter new name */
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				nk_label(gui->ctx, _l("New name:"), NK_TEXT_LEFT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, new_name, DXF_MAX_CHARS, nk_filter_default);
				
				/* Rename */
				if (nk_button_label(gui->ctx, _l("Rename"))){
					/* verify if exists other block with same name */
					if(dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", new_name)){
						snprintf(gui->log_msg, 63, _l("Error: exists Block with same name"));
					}
					else{ /* proceed to rename */
						block_rename(gui->drawing, gui->blk_name, new_name);
						/* update informations */
						strncpy(gui->blk_name, new_name, DXF_MAX_CHARS);
						blk_idx = 1;
						
						show_blk_edit = 0;
						nk_popup_close(gui->ctx);
					}
				}
				nk_label(gui->ctx, _l("New description:"), NK_TEXT_LEFT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, new_descr, DXF_MAX_CHARS, nk_filter_default);
				if (nk_button_label(gui->ctx, _l("Update"))){
					blk = dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", gui->blk_name);
					dxf_attr_change(blk, 4, (void *)new_descr);
					blk_idx = 1;
						
					show_blk_edit = 0;
					nk_popup_close(gui->ctx);
				}
				/* cancel - close popup */
				if (nk_button_label(gui->ctx, _l("Cancel"))){
					show_blk_edit = 0;
					nk_popup_close(gui->ctx);
				}
				
				nk_popup_end(gui->ctx);
			} else {
				show_blk_edit = 0;
			}
		}
	} else {
		show_blk_mng = 0;
	}
	
	
	/* **************************************************** */
	
	
	
	nk_end(gui->ctx);
	
	
	if ((show_blk_create)){ /* block creation window interface */
		if (nk_begin(gui->ctx, _l("New Block"), nk_rect(gui->next_win_x + 10, gui->next_win_y + 30, 320, 450), NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE)){
			
			/* enter new name */
			nk_layout_row_dynamic(gui->ctx, 20, 1);
			nk_label(gui->ctx, _l("Name:"), NK_TEXT_LEFT);
			nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, new_name, DXF_MAX_CHARS, nk_filter_default);
			nk_label(gui->ctx, _l("Description:"), NK_TEXT_LEFT);
			nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, gui->blk_descr, DXF_MAX_CHARS, nk_filter_default);
			nk_checkbox_label(gui->ctx, _l("Text to Attributes"), &gui->text2tag);
			if(gui->text2tag && !xref){
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				nk_label(gui->ctx, _l("Attrib. mark:"), NK_TEXT_LEFT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE, gui->tag_mark, DXF_MAX_CHARS, nk_filter_default);
				nk_label(gui->ctx, _l("Hide mark:"), NK_TEXT_LEFT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE, gui->hide_mark, DXF_MAX_CHARS, nk_filter_default);
				nk_label(gui->ctx, _l("Value mark:"), NK_TEXT_LEFT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE, gui->value_mark, DXF_MAX_CHARS, nk_filter_default);
				nk_label(gui->ctx, _l("Default value:"), NK_TEXT_LEFT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE, gui->dflt_value, DXF_MAX_CHARS, nk_filter_default);
			}
			
			/* confirm */
			
			nk_layout_row_dynamic(gui->ctx, 20, 1);
			nk_label(gui->ctx, _l("Create block from:"), NK_TEXT_LEFT);
			
			/* Tabs for select source of block, with two options:
			- From current drawing, where the user will select elements interactively;
			- From external drawing file; */
			nk_style_push_vec2(gui->ctx, &gui->ctx->style.window.spacing, nk_vec2(0,0));
			nk_layout_row_begin(gui->ctx, NK_STATIC, 20, 4);
			if (gui_tab (gui, _l("Interactive"), from == INTERACTIVE)) from = INTERACTIVE;
			if (gui_tab (gui, _l("File"), from == FROM_FILE)) from = FROM_FILE;
			nk_style_pop_vec2(gui->ctx);
			nk_layout_row_end(gui->ctx);
			
			
			
			if (from == INTERACTIVE){
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				if (nk_button_label(gui->ctx, _l("Proceed"))){
					create = 0;
					/* verify if  block name is valid */
					if(strlen(new_name) < 1){
						snprintf(gui->log_msg, 63, _l("Error: invalid Block name"));
					}
					/* verify if exists other block with same name */
					else if(dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", new_name)){
						snprintf(gui->log_msg, 63, _l("Error: exists Block with same name"));
					}
					else{ /* proceed to create */
						create = 1;
						gui->modal = NEW_BLK;
						gui->step = 0;
						strncpy(gui->blk_name, new_name, DXF_MAX_CHARS);
						
						/* close window temporaly to complete steps */
						show_blk_mng = 0;
					}
				}
			}
			else if (from == FROM_FILE){
				
				nk_layout_row(gui->ctx, NK_DYNAMIC, 22, 2, (float[]){0.5, 0.25});
				
				nk_label(gui->ctx, _l("Path:"), NK_TEXT_LEFT);
				
				
				if (nk_button_label(gui->ctx, _l("Browse"))){
					/* supported file format */
					static const char *ext_type[] = {
						"DXF",
						"*"
					};
					static char ext_descr[2][DXF_MAX_CHARS + 1];
          strncpy(ext_descr[0], _l("Drawing files (.dxf)"), DXF_MAX_CHARS);
          strncpy(ext_descr[1], _l("All files (*)"), DXF_MAX_CHARS);
					
					#define FILTER_COUNT 2
					
					show_app_file = 1;
					/* set filter for suported output formats */
					for (i = 0; i < FILTER_COUNT; i++){
						gui->file_filter_types[i] = ext_type[i];
						gui->file_filter_descr[i] = ext_descr[i];
					}
					gui->file_filter_count = FILTER_COUNT;
					gui->filter_idx = 0;
					
					gui->show_file_br = 1;
					gui->curr_path[0] = 0;
				}
				
				/* manual entry to file path */
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER|NK_EDIT_CLIPBOARD, path, DXF_MAX_CHARS, nk_filter_default);
				
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				nk_checkbox_label(gui->ctx, _l("Only reference"), &xref);
				if (xref){
					nk_checkbox_label(gui->ctx, _l("Full path"), &xref_full);
				}
				
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				if (nk_button_label(gui->ctx, _l("Create"))){
					/* verify if  block name is valid */
					if(strlen(new_name) < 1){
						snprintf(gui->log_msg, 63, _l("Error: invalid Block name"));
					}
					/* verify if exists other block with same name */
					else if(dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", new_name)){
						snprintf(gui->log_msg, 63, _l("Error: exists Block with same name"));
					}
					else{
						dxf_node *blkrec = NULL, *blk_new = NULL;
						int ok = 0;
						if (xref){
							if (xref_full){
								ok =  dxf_new_block_xref (gui->drawing, new_name, gui->blk_descr,
									"0", path, &blkrec, &blk_new, DWG_LIFE);
							} else {
								ok =  dxf_new_block_xref (gui->drawing, new_name, gui->blk_descr,
									"0", get_filename(path), &blkrec, &blk_new, DWG_LIFE);
							}
						} else {
							ok = dxf_new_blk_file (gui->drawing, new_name, gui->blk_descr,
								NULL,//(double []){0.0, 0.0, 0.0},
								gui->text2tag, gui->tag_mark, gui->hide_mark, gui->value_mark, gui->dflt_value,
								"0", path, &blkrec, &blk_new, DWG_LIFE);
						}
						if (ok){
							gui_tstyle(gui); /* add additional fonts to main list, if required */
							
							do_add_entry(&gui->list_do, _l("NEW BLOCK")); /* undo/redo list*/
							do_add_item(gui->list_do.current, NULL, blkrec); /* undo/redo list*/
							do_add_item(gui->list_do.current, NULL, blk_new); /* undo/redo list*/
							
							
							/* update informations */
							create = 1;
							blk_idx = -1;
							strncpy(gui->blk_name, new_name, DXF_MAX_CHARS);
							/* close popup */
							show_blk_create = 0;
						} else {
							snprintf(gui->log_msg, 63, _l("Error: in Block creation from file"));
						}
					}
				}
			}
			
			//nk_layout_row_dynamic(gui->ctx, 20, 2);
			
			/* cancel - close popup */
			if (nk_button_label(gui->ctx, _l("Cancel"))){
				create = 0;
				show_blk_create = 0;
				//nk_popup_close(gui->ctx);
			}
			
			/* verify creation complete */
			if (create && dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", gui->blk_name)){
				create = 0;
				/* update informations */
				blk_idx = 1;
				/* close popup */
				show_blk_create = 0;
				//nk_popup_close(gui->ctx);
			}
		} else {
			create = 0;
			show_blk_create = 0;
		}
		nk_end(gui->ctx);
	}
	
	if (show_app_file){ /* running file browser */
		if (gui->show_file_br == 2){ /* return file OK */
			/* close browser window*/
			gui->show_file_br = 0;
			show_app_file = 0;
			/* update output path */
			strncpy(path, gui->curr_path, DXF_MAX_CHARS);
		}
	}
	
	
	
	
	
	
	
	/* edit attributes definitions in block */
	if (show_attr_edit){
		static dxf_node *blk = NULL, *attr = NULL, *new_ent = NULL;
		static int init = 0;
		static dxf_node *attributes[1000];
		static int num_attr = 0;
		static char tag[1000][DXF_MAX_CHARS+1];
		static char value[1000][DXF_MAX_CHARS+1];
		static int hidden[1000];
		dxf_node *tmp, *tmp2;
		char *new_str;
		
		static char blk_name[DXF_MAX_CHARS+1];
		
		if (!new_ent){
			/* copy original block entity */
			blk = dxf_find_obj_descr2(gui->drawing->blks, "BLOCK", gui->blk_name);
			new_ent = dxf_ent_copy(blk, DWG_LIFE);
		}
		
		/* init the interface */
		if (!init){
			/* get current block form it's gui name */
			
			blk_name[0] = 0;
			if (blk){
				/* save block name */
				strncpy (blk_name, gui->blk_name, DXF_MAX_CHARS);
				/* find attibutes */
				num_attr = 0;
				while ((attr = dxf_find_obj_i(new_ent, "ATTDEF", num_attr)) && num_attr < 999){
					/* construct tables with each attribute informations */
					attributes[num_attr] = attr;
					tag[num_attr][0] = 0;
					value[num_attr][0] = 0;
					hidden[num_attr] = 0;
					if(tmp = dxf_find_attr2(attr, 2))
						strncpy(tag[num_attr], //tmp->value.s_data, DXF_MAX_CHARS);
              strpool_cstr( &name_pool, tmp->value.str), DXF_MAX_CHARS);
					if(tmp = dxf_find_attr2(attr, 1))
						strncpy(value[num_attr], //tmp->value.s_data, DXF_MAX_CHARS);
              strpool_cstr( &value_pool, tmp->value.str), DXF_MAX_CHARS);
					if(tmp = dxf_find_attr2(attr, 70))
						hidden[num_attr] = tmp->value.i_data & 1;
					
					num_attr++;
				}
				
				init = 1; /* init success */
			}
			else { /* init fail -  no block found */
				init = 0;
				new_ent = NULL;
				show_attr_edit = 0;
			}
		}
		
		if (init){
			/* edit attributes window */
			if (nk_begin(gui->ctx, _l("Edit Attributes"), nk_rect(gui->next_win_x + 100, gui->next_win_y + 100, 330, 360), NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE)){
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				nk_label_colored(gui->ctx, blk_name, NK_TEXT_CENTERED, nk_rgb(255,255,0));
				nk_layout_row_dynamic(gui->ctx, 250, 1);
				if (nk_group_begin(gui->ctx, "Attr_list", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
					/* show header */
					nk_layout_row_dynamic(gui->ctx, 32, 1);
					if (nk_group_begin(gui->ctx, "Attr_head", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
						/* dynamic width for Attribute tag and value. Fixed width for hide flags */
						nk_layout_row_template_begin(gui->ctx, 22);
						nk_layout_row_template_push_dynamic(gui->ctx);
						nk_layout_row_template_push_dynamic(gui->ctx);
						nk_layout_row_template_push_static(gui->ctx, 50);
						nk_layout_row_template_push_static(gui->ctx, 20);
						nk_layout_row_template_push_static(gui->ctx, 8);
						nk_layout_row_template_end(gui->ctx);
						
						if (nk_button_label(gui->ctx, _l("Tag"))){
							
						}
						if (nk_button_label(gui->ctx, _l("Value"))){
							
						}
						if (nk_button_label(gui->ctx, _l("Hide"))){
							
						}
						nk_group_end(gui->ctx);
					}
					/* show attributes list */
					nk_layout_row_dynamic(gui->ctx, 200, 1);
					if (nk_group_begin(gui->ctx, "Attr_names", NK_WINDOW_BORDER)) {
						/* dynamic width for Attribute tag and value. Fixed width for hide flags */
						nk_layout_row_template_begin(gui->ctx, 20);
						nk_layout_row_template_push_dynamic(gui->ctx);
						nk_layout_row_template_push_dynamic(gui->ctx);
						nk_layout_row_template_push_static(gui->ctx, 50);
						nk_layout_row_template_push_static(gui->ctx, 20);
						nk_layout_row_template_end(gui->ctx);
						
						for (i = 0; i < num_attr; i++){
							/* tag */
							nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, tag[i], DXF_MAX_CHARS, nk_filter_default);
							/* value */
							nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, value[i], DXF_MAX_CHARS, nk_filter_default);
							/* hide flag */
							if (hidden[i]){
								if (nk_button_label_styled(gui->ctx, &gui->b_icon_unsel, "X"))
									hidden[i] = 0;
							} else{
								if (nk_button_label_styled(gui->ctx, &gui->b_icon_unsel, " "))
									hidden[i] = 1;
							}
							/* delete current attribute */
							if (nk_button_image_styled(gui->ctx, &gui->b_icon, nk_image_ptr(gui->i_trash))){
								dxf_obj_subst(attributes[i], NULL);
								init = 0; /* reinit the list */
							}
						}
						nk_group_end(gui->ctx);
					}
					nk_group_end(gui->ctx);
				}
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				if (nk_button_label(gui->ctx, _l("OK"))){
					/* update changes in attributes */
					for (i = 0; i < num_attr; i++){
						new_str = trimwhitespace(tag[i]);
						/* verify if tags contain spaces */
						if (strchr(new_str, ' ')){
							snprintf(gui->log_msg, 63, _l("Error: No spaces allowed in tags"));
							continue; /* skip change */
						}
						attr = dxf_find_obj_i(new_ent, "ATTDEF", i);
						
						/* update tag */
						dxf_attr_change(attr, 2, new_str);
						/* update value */
						new_str = trimwhitespace(value[i]);
						dxf_attr_change(attr, 1, new_str);
						/* update hide flag */
						dxf_attr_change(attr, 70, &hidden[i]);
						
					}
					dxf_obj_subst(blk, new_ent);
					/* add to undo/redo list */
					do_add_entry(&gui->list_do, _l("Edit Block Attributes"));
					do_add_item(gui->list_do.current, blk, new_ent);
					
					blk_idx = 1; /* update block preview */
					/* close edit window */
					init = 0;
					new_ent = NULL;
					show_attr_edit = 0;
				}
				if (nk_button_label(gui->ctx, _l("Cancel"))){
					/* close edit window */
					init = 0;
					new_ent = NULL;
					show_attr_edit = 0;
				}
			} else {
				/* close edit window */
				init = 0;
				new_ent = NULL;
				show_attr_edit = 0;
			}
			nk_end(gui->ctx);
		}
	}
	
	return show_blk_mng;
}

int block_use(dxf_drawing *drawing){
	/* count blocks in use in drawing */
	int ok = 0, i;
	dxf_node *current, *prev, *obj = NULL, *list[2];
  
  STRPOOL_U64 sys_blk[2];
  
  sys_blk[0] = strpool_inject( &name_pool, "*MODEL_SPACE", strlen("*MODEL_SPACE") );
  sys_blk[1] = strpool_inject( &name_pool, "*PAPER_SPACE", strlen("*PAPER_SPACE") );
	
	list[0] = NULL; list[1] = NULL;
	if (drawing){
		list[0] = drawing->ents;
		list[1] = drawing->blks;
	}
	else return 0;
	
	/* init blocks count */
	current = drawing->blks;
	if (current) current = current->obj.content;
	while (current){ /* sweep elements in section */
		if (current->type == DXF_ENT){
			//if (strcmp(current->obj.name, "BLOCK") == 0){
      if (dxf_ident_ent_type(current) == DXF_BLK){
				/* uses block's layer index to count */
				current->obj.layer= 0;
				
				/* get name of current block */
				dxf_node * blk_nm = dxf_find_attr2(current, 2);
				if (blk_nm){
					//char name[DXF_MAX_CHARS + 1];
					//strncpy(name, blk_nm->value.s_data, DXF_MAX_CHARS);
					//str_upp(name);
					/* mark used if is a system block*/
					//if (strcmp(name, "*MODEL_SPACE") == 0) current->obj.layer= 1;
					//else if (strcmp(name, "*PAPER_SPACE") == 0) current->obj.layer= 1;
          for (i = 0; i< 2; i++){
            if (blk_nm->value.str == sys_blk[i]) current->obj.layer= 1;
          }
				}
			}
		}
		current = current->next; /* go to the next in the list*/
	}
	
	for (i = 0; i< 2; i++){ /* look in BLOCKS and ENTITIES sections */
		obj = list[i];
		current = obj;
		while (current){ /* sweep elements in section */
			ok = 1;
			prev = current;
			if (current->type == DXF_ENT){
				//if ((strcmp(current->obj.name, "INSERT") == 0) ||
				//	(strcmp(current->obj.name, "DIMENSION")) == 0){
        if (dxf_ident_ent_type(current) == DXF_INSERT ||
          dxf_ident_ent_type(current) == DXF_DIMENSION){
					dxf_node *block = NULL, *blk_name = NULL;
					blk_name = dxf_find_attr2(current, 2);
					if(blk_name) {
						block = dxf_find_obj_descr2(drawing->blks, "BLOCK", //blk_name->value.s_data);
              (char*) strpool_cstr( &name_pool, blk_name->value.str));
						if(block) {
							/* uses block's layer index to count */
							block->obj.layer++;
						}
					}
				}
				/* search also in sub elements */
				if (current->obj.content){
					/* starts the content sweep */
					current = current->obj.content;
					continue;
				}
			}
				
			current = current->next; /* go to the next in the list*/
			
			if ((prev == NULL) || (prev == obj)){ /* stop the search if back on initial entity */
				current = NULL;
				break;
			}

			/* ============================================================= */
			while (current == NULL){
				/* end of list sweeping */
				if ((prev == NULL) || (prev == obj)){ /* stop the search if back on initial entity */
					//printf("para\n");
					current = NULL;
					break;
				}
				/* try to back in structure hierarchy */
				prev = prev->master;
				if (prev){ /* up in structure */
					/* try to continue on previous point in structure */
					current = prev->next;
					
				}
				else{ /* stop the search if structure ends */
					current = NULL;
					break;
				}
			}
		}
	}
	
	return ok;
}

int block_rename(dxf_drawing *drawing, char *curr_name, char *new_name){
	/* Rename block and its relative inserts and dimensions */
	int ok = 0, i;
	dxf_node *current, *prev, *obj = NULL, *list[2];
	
	list[0] = NULL; list[1] = NULL;
	if (drawing){
		list[0] = drawing->ents;
		list[1] = drawing->blks;
	}
	else return 0;
	
	if (!curr_name || !new_name) return 0;
	if (strlen(curr_name) == 0 || strlen (new_name) == 0) return 0;
	
	
	/* copy string for secure manipulation */
	//char old_cpy[DXF_MAX_CHARS], *curr_name_cpy;
	//char new_cpy[DXF_MAX_CHARS], *new_name_cpy;
	//strncpy(old_cpy, curr_name, DXF_MAX_CHARS);
	//strncpy(new_cpy, new_name, DXF_MAX_CHARS);
	/* remove trailing spaces */
	//curr_name_cpy = trimwhitespace(old_cpy);
	//new_name_cpy = trimwhitespace(new_cpy);
  STRPOOL_U64 old = strpool_inject( &name_pool, (char const*) curr_name, strlen(curr_name) );
  STRPOOL_U64 rpl = strpool_inject( &name_pool, (char const*) new_name, strlen(new_name) );
	
	
	/* first, rename main block object */
	//current = dxf_find_obj_descr2(drawing->blks, "BLOCK", curr_name_cpy);
  current = dxf_find_obj_descr2(drawing->blks, "BLOCK", curr_name);
	if(current) {
		//dxf_attr_change(current, 2, new_name_cpy);
    dxf_attr_change(current, 2, new_name);
	}
	else return 0;
	
	/* then, rename block_record object*/
	//current = dxf_find_obj_descr2(drawing->blks_rec, "BLOCK_RECORD", curr_name_cpy);
  current = dxf_find_obj_descr2(drawing->blks_rec, "BLOCK_RECORD", curr_name);
	if(current) {
		//dxf_attr_change(current, 2, new_name_cpy);
    dxf_attr_change(current, 2, new_name);
	}	
	
	/* change to upper case for  consistent comparison*/
	//char curr_upp[DXF_MAX_CHARS];
	//strncpy(curr_upp, curr_name_cpy, DXF_MAX_CHARS);
	//str_upp(curr_upp);
	
	for (i = 0; i< 2; i++){ /* look in BLOCKS and ENTITIES sections */
		obj = list[i];
		current = obj;
		while (current){ /* sweep elements in section */
			ok = 1;
			prev = current;
			if (current->type == DXF_ENT){
				//if ((strcmp(current->obj.name, "INSERT") == 0) ||
				//	(strcmp(current->obj.name, "DIMENSION")) == 0){
        if (dxf_ident_ent_type(current) == DXF_INSERT ||
          dxf_ident_ent_type(current) == DXF_DIMENSION){
					dxf_node *block = NULL, *blk_name = NULL;
					blk_name = dxf_find_attr2(current, 2);
					if(blk_name) {
						/* change to upper case for  consistent comparison*/
						//char name_upp[DXF_MAX_CHARS];
						//strncpy(name_upp, blk_name->value.s_data, DXF_MAX_CHARS);
						//str_upp(name_upp);
						
						/* verify if is a looking name*/
						//if (strcmp(name_upp, curr_upp) == 0){
            if (blk_name->value.str == old){
							/* change to new name */
							//blk_name->value.s_data[0] = 0;
							//strncpy(blk_name->value.s_data, new_name_cpy, DXF_MAX_CHARS);
              blk_name->value.str = rpl;
						}
					}
				}
				/* search also in sub elements */
				if (current->obj.content){
					/* starts the content sweep */
					current = current->obj.content;
					continue;
				}
			}
				
			current = current->next; /* go to the next in the list*/
			
			if ((prev == NULL) || (prev == obj)){ /* stop the search if back on initial entity */
				current = NULL;
				break;
			}

			/* ============================================================= */
			while (current == NULL){
				/* end of list sweeping */
				if ((prev == NULL) || (prev == obj)){ /* stop the search if back on initial entity */
					//printf("para\n");
					current = NULL;
					break;
				}
				/* try to back in structure hierarchy */
				prev = prev->master;
				if (prev){ /* up in structure */
					/* try to continue on previous point in structure */
					current = prev->next;
					
				}
				else{ /* stop the search if structure ends */
					current = NULL;
					break;
				}
			}
		}
	}
	
	return ok;
}

int block_select(gui_obj *gui, char *name){
	/* add to selection inserts and dimensions relative to block name */
	int ok = 0, i;
	dxf_node *current;
	
	if (!name || !gui) return 0;
	if (strlen(name) == 0) return 0;
  
  STRPOOL_U64 look = strpool_inject( &name_pool, (char const*) name, strlen(name) );
	
	/* copy string for secure manipulation */
	//char name_cpy[DXF_MAX_CHARS], *name_upp;
	//strncpy(name_cpy, name, DXF_MAX_CHARS);
	/* remove trailing spaces and change to upper case for  consistent comparison*/
	//name_upp = trimwhitespace(name_cpy);
	//str_upp(name_upp);
	
	current = gui->drawing->ents->obj.content;
	while (current){ /* sweep elements in section */
		ok = 1;
		if (current->type == DXF_ENT){
			/*verify if entity layer is on and thaw */
			if ((!gui->drawing->layers[current->obj.layer].off) && 
				(!gui->drawing->layers[current->obj.layer].frozen)){
				/* look for inserts and dimensions */
				//if ((strcmp(current->obj.name, "INSERT") == 0) ||
				//	(strcmp(current->obj.name, "DIMENSION")) == 0){
        if (dxf_ident_ent_type(current) == DXF_INSERT ||
          dxf_ident_ent_type(current) == DXF_DIMENSION){
					dxf_node *block = NULL, *blk_name = NULL;
					blk_name = dxf_find_attr2(current, 2);
					if(blk_name) {
						/* change to upper case for  consistent comparison*/
						//char blk_name_upp[DXF_MAX_CHARS];
						//strncpy(blk_name_upp, blk_name->value.s_data, DXF_MAX_CHARS);
						//str_upp(blk_name_upp);
						
						/* verify if is a looking name*/
						//if (strcmp(name_upp, blk_name_upp) == 0){
            if (blk_name->value.str == look){
							/* add to selection list */
							list_modify(gui->sel_list, current, LIST_ADD, SEL_LIFE);
							
						}
					}
				}
			}
		}
			
		current = current->next; /* go to the next in the list*/
	}
	
	return ok;
}