#ifndef COAL_OUT_H
#define COAL_OUT_H

qoutput_fn out_json_data;
qoutput_fn out_json_tree;
qoutput_fn out_json_search;
qoutput_fn out_json_invalid;

qoutput_fn out_bin_data;
qoutput_fn out_bin_tree;
qoutput_fn out_bin_search;
qoutput_fn out_bin_invalid;

qoutput_fn out_lines_data;
qoutput_fn out_lines_tree;
qoutput_fn out_lines_search;
qoutput_fn out_lines_invalid;

#define buf_check( _p, _mk )		if( _p > _mk )\
	{\
		s->out.len = _p - buf;\
		net_write_data( s );\
		_p = buf;\
	}


#endif
