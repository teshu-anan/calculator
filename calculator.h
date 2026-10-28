#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <QWidget>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class Calculator; }
QT_END_NAMESPACE

class QKeyEvent;
class QPushButton;

/*
 * 带键盘事件的计算器
 *
 * 设计要点：
 * 1. 鼠标点击和键盘按键走同一个入口 handleToken()，保证两种输入行为完全一致；
 * 2. 用一个显式的状态机（State）记录“现在到底在输第几个操作数”，
 *    避免各种边界情况（连续运算、结果后继续输入、除零等）互相干扰；
 * 3. 所有按钮共用一个槽函数，靠按钮文本分发，减少重复代码。
 */
class Calculator : public QWidget
{
    Q_OBJECT

public:
    explicit Calculator(QWidget *parent = nullptr);
    ~Calculator() override;

protected:
    // 重写键盘事件：把物理按键翻译成和鼠标点击一样的“输入记号”
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onButtonClicked();          // 所有按钮共用的槽

private:
    // ---------- 输入状态机 ----------
    enum class State {
        EnteringLeft,    // 正在输入第一个操作数（没有挂起的运算符）
        EnteringRight,   // 已按下运算符，正在输入第二个操作数
        ShowingResult,   // 刚按下“=”，显示的是计算结果
        Error            // 出错状态（例如除数为 0）
    };

    // ---------- 统一输入入口 ----------
    void handleToken(const QString &token);   // 鼠标与键盘共用的分发函数
    void appendDigit(const QChar &digit);
    void appendDot();
    void setOperator(const QString &op);
    void handleEquals();
    void handleBackspace();
    void handleClear();

    // ---------- 内部辅助 ----------
    bool applyPendingOperation();             // 执行挂起的四则运算
    void startNewInput();                     // 清空全部状态，开始一次新的计算
    void updateDisplay();                     // 刷新数字显示区
    void updateHistory();                     // 刷新上方算式显示区
    void showError(const QString &message);   // 进入错误状态并提示
    static QString normalizeOperator(const QChar &ch);   // × ÷ - 统一成 * / -
    static QString displayOperator(const QString &op);   // * / 反显成 × ÷
    static QString formatNumber(double value);           // 去掉多余的 0，避免 0.30000000000000004

    static const int kMaxDigits = 15;         // 单个操作数最多允许输入的位数

    Ui::Calculator *ui;
    State   m_state       = State::EnteringLeft;
    QString m_input;                          // 当前正在输入的数字文本（显示区内容）
    double  m_leftOperand = 0.0;               // 第一个操作数（也可当作中间结果）
    QString m_operator;                        // 挂起的运算符，空串表示没有
    bool    m_freshInput  = true;              // true 表示下一个数字要“重新开始一个数”
};

#endif // CALCULATOR_H
