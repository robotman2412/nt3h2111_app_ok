/*
	MIT License

	Copyright (c) 2023 Julian Scheffers

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "ui.h"
#include "main.h"
#include "theme.h"
#include "nt3h2111.h"
#include "ndef.h"
#include "ndef_record_types.h"

// Gets the NDEF datas for U.
ndef_ctx get_ndef() {
	size_t len;
	uint8_t *buf;
	int res = nt3h2111_get_ndef(&tag, &len, &buf);
	if (res) return NULL;
	ndef_ctx ndef = ndef_decode(buf, &len);
	free(buf);
	return ndef;
}

// Draw a hexdump.
pax_vec2f draw_hexdump(float dx, float dy, const void *data, size_t len) {
	const uint8_t *bytes = data;
	const uint_fast8_t cols = hex_cols;
	const char *hexc = "0123456789ABCDEF";
	char tmp[3];
	
	float w = 0, h = 0;
	
	float adx = dx;
	if (cols < len) {
		adx += (cols*3+2)*mono_w;
	} else {
		adx += (len*3+2)*mono_w;
	}
	
	for (size_t y = 0; y*cols < len; y++) {
		// HEX representation.
		for (uint_fast8_t x = 0; x < cols && x+y*cols < len; x++) {
			size_t i = y*cols + x;
			tmp[0] = hexc[bytes[i] >> 4];
			tmp[1] = hexc[bytes[i] & 15];
			tmp[2] = 0;
			float x0 = dx+x*3*mono_w, y0 = dy+y*mono_h;
			pax_vec2f dims = pax_draw_text(&buf, fg_col, mono, font_size, x0, y0, tmp);
			if (dims.x + x0 - dx > w) w = dims.x + x0 - dx;
		}
		
		// ASCII representation.
		for (uint_fast8_t x = 0; x < cols && x+y*cols < len; x++) {
			char c = bytes[y*cols + x];
			tmp[0] = c >= 0x20 && c <= 0x7e ? c : '.';
			tmp[1] = 0;
			float x0 = adx+x*mono_w, y0 = dy+y*mono_h;
			pax_vec2f dims = pax_draw_text(&buf, fg_col, mono, font_size, x0, y0, tmp);
			if (dims.x + x0 - dx > w) w = dims.x + x0 - dx;
		}
		
		h += mono_h;
	}
	
	return (pax_vec2f) { w, h };
}

// Set smart poster menu.
void menu_set_smartposter() {
	
}

// Display the summary of one record.
static float draw_record_summary(ndef_record record, bool detailed) {
	if (ndef_record_is_smartposter(record)) {
		ndef_smartposter sp = ndef_record_get_smartposter(record);
		pax_draw_text(&buf, fg_col, font, font_size, 0,  0*font_size, "Smart poster:");
		pax_draw_text(&buf, fg_col, font, font_size, 10, 1*font_size, "URI:");
		pax_draw_text(&buf, fg_col, font, font_size, 50, 1*font_size, sp.uri);
		pax_draw_text(&buf, fg_col, font, font_size, 10, 2*font_size, "Text:");
		pax_draw_text(&buf, fg_col, font, font_size, 50, 2*font_size, sp.text.text);
		ndef_smartposter_destroy(sp);
		return 3*font_size;
		
	} else if (ndef_record_is_text(record)) {
		ndef_text text = ndef_record_get_text(record);
		pax_draw_text(&buf, fg_col, font, font_size, left_x, 0*font_size, "Text:");
		pax_draw_text(&buf, fg_col, font, font_size, data_x, 0*font_size, text.text);
		ndef_text_destroy(text);
		return 1*font_size;
		
	} else if (ndef_record_is_uri(record)) {
		char *text = ndef_record_get_uri(record);
		pax_draw_text(&buf, fg_col, font, font_size, left_x, 0*font_size, "URI:");
		pax_draw_text(&buf, fg_col, font, font_size, data_x, 0*font_size, text);
		free(text);
		return 1*font_size;
		
	} else if (record.tnf == NDEF_TNF_EMPTY || (!record.id_len && !record.payload_len && !record.type_len)) {
		pax_draw_text(&buf, fg_col, font, font_size, left_x, 0*font_size, "(empty record)");
		return 1*font_size;
		
	} else {
		float h = font_size;
		pax_draw_text(&buf, fg_col, font, font_size, left_x, 0, "Record:");
		pax_draw_text(&buf, fg_col, font, font_size, data_x, 0, ndef_tnf_names[record.tnf]);
		if (record.type_len) {
			pax_draw_text(&buf, fg_col, font, font_size, left_x + indent_x, h, "Type:");
			h += draw_hexdump(data_x, h, record.type, record.type_len).y;
		}
		if (record.payload_len) {
			pax_draw_text(&buf, fg_col, font, font_size, left_x + indent_x, h, "Payload:");
			// char tmp[32];
			// snprintf(tmp, sizeof(tmp)-1, "(%zu byte%s)", record.payload_len, record.payload_len == 1 ? "" : "s");
			// h += font_size;
			h += draw_hexdump(data_x, h, record.payload, record.payload_len).y;
		}
		if (record.id_len) {
			pax_draw_text(&buf, fg_col, font, font_size, left_x + indent_x, h, "ID:");
			h += draw_hexdump(data_x, h, record.id, record.id_len).y;
		}
		return h;
	}
}

// Show summary menu.
void menu_show_summary() {
	ndef_ctx ndef = get_ndef();
	
	// Total height of all the things.
	float draw_h = 0;
	// Fraction of scrolling.
	float scroll = 0;
	
	pax_background(&buf, 0);
	pax_push_2d(&buf);
	
	// Draw the title.
	pax_center_text(&buf, fg_col, title, title_size, pax_buf_get_widthf(&buf)/2, 0, "NFC tag: Summary");
	
	// Set scroll.
	float scroll_h = pax_buf_get_heightf(&buf) - draw_h;
	if (scroll_h > 0) scroll_h = 0;
	pax_apply_2d(&buf, matrix_2d_translate(0, (int) (scroll * scroll_h)));
	pax_clip(&buf, 0, title_size, pax_buf_get_width(&buf), pax_buf_get_height(&buf) - title_size);
	
	// Draw the content.
	draw_h = title_size;
	pax_apply_2d(&buf, matrix_2d_translate(0, title_size));
	for (size_t i = 0; i < ndef_records_len(ndef); i++) {
		float dy = draw_record_summary(ndef_records(ndef)[i], true);
		pax_apply_2d(&buf, matrix_2d_translate(0, dy));
		draw_h += dy;
	}
	
	pax_noclip(&buf);
	pax_pop_2d(&buf);
	disp_flush();
	
	ndef_destroy(ndef);
}

// Show NDEF hierarchy menu.
void menu_show_ndef() {
	ndef_ctx ndef = get_ndef();
	
	// Total height of all the things.
	float draw_h = 0;
	// Fraction of scrolling.
	float scroll = 0;
	
	ndef_destroy(ndef);
}

