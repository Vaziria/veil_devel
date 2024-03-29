#include <qt/veil/settings/settingsminting.h>
#include <qt/veil/forms/ui_settingsminting.h>

#include <util/system.h>
#include <qt/bitcoinunits.h>
#include <qt/guiconstants.h>
#include <qt/walletmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <QRegExpValidator>

#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <iostream>
#include <qt/veil/qtutils.h>

SettingsMinting::SettingsMinting(QWidget *parent, WalletView *mainWindow, WalletModel *_walletModel) :
    QDialog(parent),
    ui(new Ui::SettingsMinting),
    walletModel(_walletModel),
    mainWindow(mainWindow)
{
    ui->setupUi(this);
    ui->btnEsc->setProperty("cssClass" , "btn-text-primary-inactive");
    ui->useBasecoin->setProperty("cssClass" , "btn-check");

    ui->btnMint->setProperty("cssClass" , "btn-text-primary");

    ui->btnEsc->setProperty("cssClass" , "btn-text-primary-inactive");

    ui->editAmount->setPlaceholderText("Enter amount here (multiple of 10)");
    ui->editAmount->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->editAmount->setProperty("cssClass" , "edit-primary");

    // Balance
    interfaces::Wallet& wallet = walletModel->wallet();
    interfaces::WalletBalances balances = wallet.getBalances();
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    ui->labelZVeilBalance->setText(BitcoinUnits::formatWithUnit(unit, balances.zerocoin_balance, false, BitcoinUnits::separatorAlways));
    ui->labelConvertableCt->setText(BitcoinUnits::formatWithUnit(unit,balances.ct_balance, false, BitcoinUnits::separatorAlways));
    ui->labelConvertableBasecoin->setText(BitcoinUnits::formatWithUnit(unit,balances.basecoin_balance, false, BitcoinUnits::separatorAlways));
    ui->labelConvertibleRingCt->setText(BitcoinUnits::formatWithUnit(unit,balances.ring_ct_balance, false, BitcoinUnits::separatorAlways));

    CAmount nMinDenom = libzerocoin::ZerocoinDenominationToAmount(libzerocoin::CoinDenomination::ZQ_TEN);

    if (balances.basecoin_balance < nMinDenom) {
        ui->warnLabel->setText(ui->warnLabel->text().arg(libzerocoin::CoinDenomination::ZQ_TEN + 0.1));
        ui->warnLabel->setVisible(true);
        ui->btnSendMint->setEnabled(false);
    } else {
        ui->warnLabel->setVisible(false);
        ui->btnSendMint->setEnabled(true);
    }

    //retrieved saved settings if nautomintdenom is not in use
    QSettings* settings = getSettings();
    if (!gArgs.IsArgSet("-nautomintdenom") && settings->contains("nAutomintDenom")){
		int tempPref = settings->value("nAutomintDenom").toInt();
		if(tempPref != nPreferredDenom && tempPref != 0){
			nPreferredDenom = tempPref;
		}
	}

    switch (nPreferredDenom){
        case 10:
            ui->radioButton10->setChecked(true);
            break;
        case 100:
            ui->radioButton100->setChecked(true);
            break;
        case 1000:
            ui->radioButton1000->setChecked(true);
            break;
        case 10000:
            ui->radioButton100000->setChecked(true);
            break;
        case -1:
            ui->checkAutomintInstant->setChecked(true);
            break;
    }

    ui->errorMessage->setVisible(false);

    // one digit between 1 and 9 followed by 0 to 10 digits followed by a zero (as it must
    // be a multiple of 10
    QRegExp rx("[1-9]\\d{1,11}");
    ui->editAmount->setValidator(new QRegExpValidator(rx, this) );

    connect(ui->btnEsc,SIGNAL(clicked()),this, SLOT(close()));
    connect(ui->btnSendMint,SIGNAL(clicked()),this, SLOT(btnMint()));
    connect(ui->editAmount, SIGNAL(returnPressed()), this, SLOT(btnMint()));
    if(gArgs.IsArgSet("-nautomintdenom")){
    	ui->labelAutomintConfig->setVisible(true);
    	ui->radioButton10->setEnabled(0);
    	ui->radioButton100->setEnabled(0);
    	ui->radioButton1000->setEnabled(0);
    	ui->radioButton100000->setEnabled(0);
    	ui->checkAutomintInstant->setEnabled(0);
    }
    else{
    	ui->labelAutomintConfig->setVisible(false);
    	connect(ui->radioButton10, SIGNAL(toggled(bool)), this, SLOT(onCheck10Clicked(bool)));
    	connect(ui->radioButton100, SIGNAL(toggled(bool)), this, SLOT(onCheck100Clicked(bool)));
    	connect(ui->radioButton1000, SIGNAL(toggled(bool)), this, SLOT(onCheck1000Clicked(bool)));
    	connect(ui->radioButton100000, SIGNAL(toggled(bool)), this, SLOT(onCheck100000Clicked(bool)));
    	connect(ui->checkAutomintInstant, SIGNAL(toggled(bool)), this, SLOT(onCheckFullMintClicked(bool)));
    }
    connect(ui->editAmount, SIGNAL(textChanged(const QString &)), this, SLOT(mintAmountChange(const QString &)));

}

/**
 * Parse a string into a number of base monetary units and
 * return validity.
 * @note Must return 0 if !valid.
 */
CAmount SettingsMinting::parseAmount(const QString &text, bool& valid_out, std::string& strError) const {
    CAmount val = 0;
    valid_out = false;
    if (!BitcoinUnits::parse(BitcoinUnits::VEIL, text, &val)) {
        strError = "failed to parse";
        return 0;
    }

    CAmount nMinDenom = libzerocoin::ZerocoinDenominationToAmount(libzerocoin::CoinDenomination::ZQ_TEN);
    if (val < nMinDenom || val > BitcoinUnits::maxMoney()) {
        strError = "not in valid range";
        return 0;
    }

    if (val % nMinDenom != 0) {
        strError = "not multiple of 10";
        return 0;
    }

    valid_out = true;
    return val;
}

void SettingsMinting::mintAmountChange(const QString &amount){

}

void SettingsMinting::btnMint(){
    mintzerocoins();
}

void SettingsMinting::mintzerocoins()
{
    // check if wallet is unlocked..
    interfaces::Wallet& wallet = walletModel->wallet();
    if (wallet.isLocked() || wallet.isUnlockedForStakingOnly()) {
        openToastDialog("Wallet Is Locked.", this);
        return;
    }

    bool isAmountValid = false;
    std::string strError;
    CAmount nAmount = parseAmount(ui->editAmount->text(), isAmountValid, strError);

    if (!isAmountValid) {
        // notify user
        std::string strMessage = strprintf("Invalid Amount: %s", strError);
        openToastDialog(strMessage.c_str(), this);
        return;
    }

    bool fUseBasecoin = ui->useBasecoin->isChecked();
    std::vector<CDeterministicMint> vDMints;
    std::vector<COutPoint> vOutpts;
    OutputTypes inputtype = OUTPUT_NULL;

    interfaces::WalletBalances balances = wallet.getBalances();
    if (!fUseBasecoin) {
        if (balances.ring_ct_balance > nAmount && chainActive.Tip()->nAnonOutputs > 20)
            inputtype = OUTPUT_RINGCT;
        else if (balances.ct_balance > nAmount)
            inputtype = OUTPUT_CT;
    } else if (balances.basecoin_balance > nAmount) {
        inputtype = OUTPUT_STANDARD;
    }

    if (inputtype == OUTPUT_NULL) {
        openToastDialog("Insufficient Balance", this);
        return;
    }

    strError = wallet.mintZerocoin(nAmount, vDMints, inputtype, nullptr);

    if(strError.empty()){
        openToastDialog("Mint completed", this);
    } else{
        openToastDialog(QString::fromStdString(strError), this);
    }
}

void SettingsMinting::onCheck10Clicked(bool res) {
    if(res && nPreferredDenom != 10){
        nPreferredDenom = 10;
        saveSettings(nPreferredDenom);
    }
}

void SettingsMinting::onCheck100Clicked(bool res){
    if(res && nPreferredDenom != 100){
        nPreferredDenom = 100;
        saveSettings(nPreferredDenom);
    }
}

void SettingsMinting::onCheck1000Clicked(bool res){
    if(res && nPreferredDenom != 1000){
        nPreferredDenom = 1000;
        saveSettings(nPreferredDenom);
    }
}

void SettingsMinting::onCheck100000Clicked(bool res){
    if(res && nPreferredDenom != 10000){
        nPreferredDenom = 10000;
        saveSettings(nPreferredDenom);
    }
}

void SettingsMinting::onCheckFullMintClicked(bool res){
    if(res && nPreferredDenom != -1){
        nPreferredDenom = -1;
        saveSettings(nPreferredDenom);
    }
}

void SettingsMinting::saveSettings(int prefDenom){
    QSettings* settings = getSettings();
    settings->setValue("nAutomintDenom", prefDenom);
    settings->sync();
}

SettingsMinting::~SettingsMinting()
{
    delete ui;
}
