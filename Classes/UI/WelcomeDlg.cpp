#include "WelcomeDlg.h"
#include "AlertDlg.h"
#include "PlayScene.h"

using namespace std;

WelcomeDlg::WelcomeDlg() : IDialog()
{
    RegDialogCtrl("Button_Logon", m_btnLogon);
}

WelcomeDlg::~WelcomeDlg(){
}

void WelcomeDlg::onUILoaded() {
    m_btnLogon->addClickEventListener([this](Ref* sender) {
        auto alert = AlertDlg::create();
        alert->setAlertType(AlertDlg::ENUM_ALERT);
        alert->setCallback([]() {
            DialogManager::shared()->closeAllDialog();
            Director::getInstance()->replaceScene(PlayScene::create());
        }, nullptr);
        alert->setText( "欢迎参加IHFE麻将游戏实验！\n请在充分了解实验流程和游戏规则后开始游戏" );
        DialogManager::shared()->showDialog(alert);
    });
}

