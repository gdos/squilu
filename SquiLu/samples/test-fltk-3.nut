function _tr(str){ return str;}

class Fl_Data_Table extends Flv_Table {
	_forPrint = null;
	
	constructor(px, py, pw, ph, pl=""){
		base.constructor(px, py, pw, ph, pl);
		_forPrint=false;
	}
}
	
//class BaseReportA4 extends Fl_Group {
class BaseReportA4 extends Fl_Window {
	rpt_Logo=null;
	rpt_Title=null;
	rpt_SubTitle=null;
	rpt_Body=null;
	rpt_SubFooter=null;
	rpt_Footer=null;
	
constructor() {
	base.constructor(10, 50, 420, 594, _tr("BaseReportA4"));
begin();
{ rpt_Logo = new Fl_Box(10, 5, 105, 35, _tr("Ourbiz"));
  rpt_Logo->labeltype(FL_SHADOW_LABEL);
  rpt_Logo->labelfont(3);
  rpt_Logo->labelsize(30);
  rpt_Logo->align((644|FL_ALIGN_INSIDE));
} // Fl_Box* rpt_Logo
{ rpt_Title = new Fl_Box(120, 5, 295, 20, _tr("Report Title"));
  rpt_Title->labelfont(1);
  rpt_Title->labelsize(16);
} // Fl_Box* rpt_Title
{ rpt_SubTitle = new Fl_Box(120, 30, 290, 10, _tr("Date"));
  rpt_SubTitle->labelsize(8);
  rpt_SubTitle->align((FL_ALIGN_RIGHT|FL_ALIGN_INSIDE));
} // Fl_Box* rpt_SubTitle
{ local o = new Fl_Box(10, 40, 405, 1, _tr("Line"));
  o->box(FL_FLAT_BOX);
  o->color(34);
  o->labeltype(FL_NO_LABEL);
} // Fl_Box* o
{ local o = rpt_Body = new Fl_Data_Table(10, 45, 408, 495);
  rpt_Body->box(FL_NO_BOX);
  rpt_Body->color(FL_BACKGROUND2_COLOR);
  rpt_Body->selection_color(FL_SELECTION_COLOR);
  rpt_Body->labeltype(FL_NORMAL_LABEL);
  rpt_Body->labelfont(0);
  rpt_Body->labelsize(8);
  rpt_Body->labelcolor(FL_FOREGROUND_COLOR);
  rpt_Body->textsize(8);
  rpt_Body->align((FL_ALIGN_BOTTOM));
  rpt_Body->when(FL_WHEN_RELEASE_ALWAYS);
  Fl_Group.current()->resizable(rpt_Body);
  o->has_scrollbar(FLVS_NONE);
  o->feature_remove(FLVF_ROW_SELECT | FLVF_PERSIST_SELECT);
  o->_forPrint = true;
} // Fl_Data_Table* rpt_Body
{ local o = new Fl_Box(10, 545, 405, 1, _tr("Line"));
  o->box(FL_FLAT_BOX);
  o->color(34);
  o->labeltype(FL_NO_LABEL);
} // Fl_Box* o
{ rpt_SubFooter = new Fl_Box(10, 547, 405, 10, _tr("Page"));
  rpt_SubFooter->labelfont(1);
  rpt_SubFooter->labelsize(8);
  rpt_SubFooter->align((FL_ALIGN_CENTER|FL_ALIGN_INSIDE));
} // Fl_Box* rpt_SubFooter
{ rpt_Footer = new Fl_Box(10, 560, 405, 19, _tr("OURBIZ is an excelente application to manage a business."));
  rpt_Footer->labelfont(2);
  rpt_Footer->labelsize(8);
  rpt_Footer->align((129|FL_ALIGN_INSIDE));
} // Fl_Box* rpt_Footer
end();
}
}
//local win = new MainWindow(10, 50, 330, 320, "OURBIZ");
local win = new BaseReportA4();
win->resizable(win);
win->show_main();

math.number_format_set_dec_point(",");
math.number_format_set_thousand_sep(".");
Fl_Input.default_number_format("\0x02,.");
//Fl:scheme("plastic");
Fl.scheme("gtk+");
//use partial match to find verdana font
Fl.visual(FL_RGB);
//allow arrow keys navigation
Fl.option(Fl.OPTION_ARROW_FOCUS, true);

Fl.run();
