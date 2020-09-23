//
// Created by Clark on 2020/9/15.
//  游戏设置层

#ifndef COCOSTUDIO_MAHJONG_CONFIDENCE_H
#define COCOSTUDIO_MAHJONG_CONFIDENCE_H

#include "IDialog.h"

class ConfidenceDlg : public IDialog<Layer>
{
public:
	ConfidenceDlg();
	virtual ~ConfidenceDlg();
	CREATE_FUNC(ConfidenceDlg);

protected:
	virtual const char* csbName() const { return "res/ConfidenceLayer.csb"; }
	virtual void onUILoaded();

private:
	Button* m_btnConfirm;
	ui::TextField * m_pConfidenceField;   //声音滑动条
	std::string confidence;
};

#endif //COCOSTUDIO_MAHJONG_SETLAYER_H
#pragma once
