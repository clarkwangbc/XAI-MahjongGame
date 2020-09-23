//
// Created by Clark on 2020/9/15
//  游戏设置层

#ifndef COCOSTUDIO_MAHJONG_SETLAYER_H
#define COCOSTUDIO_MAHJONG_SETLAYER_H

#include "IDialog.h"

class RecordDlg : public IDialog<Layer>
{
public:
	RecordDlg();
	virtual ~RecordDlg();
	CREATE_FUNC(RecordDlg);

protected:
	virtual const char* csbName() const { return "res/SetLayer.csb"; }
	virtual void onUILoaded();

private:
	Button* m_btnClose;
	ui::Slider * m_pSliderVolume;   //声音滑动条
};

#endif //COCOSTUDIO_MAHJONG_SETLAYER_H#pragma once
