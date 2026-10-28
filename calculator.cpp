#include "calculator.h"
#include "ui_calculator.h"

#include <QKeyEvent>
#include <QList>
#include <QPushButton>

Calculator::Calculator(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Calculator)
{
    ui->setupUi(this);

    // ---- 按钮初始化：18 个按钮只写 1 条 connect ----
    // 槽函数里用 sender() 取到被点的按钮，再用它的 text() 分发，避免 18 段重复代码
    const QList<QPushButton *> buttons = findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        // 按钮不接收焦点：焦点留在窗口上，键盘事件才会稳定地进入 keyPressEvent
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QPushButton::clicked, this, &Calculator::onButtonClicked);
    }

    setFocusPolicy(Qt::StrongFocus);   // 让窗口本身可以接收键盘事件
    startNewInput();                   // 初始显示 0
    setFocus();
}

Calculator::~Calculator()
{
    delete ui;
}

// =====================================================================
//  槽函数：鼠标点击的总入口
// =====================================================================
void Calculator::onButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;

    // 按钮文字本身就是“记号”：数字、"."、"+"、"-"、"×"、"÷"、"C"、"⌫"、"="
    handleToken(button->text());
}

// =====================================================================
//  键盘事件：把物理按键翻译成同样的记号，交给 handleToken()
//  —— 这样鼠标与键盘走的是同一套处理逻辑，行为不可能不一致
// =====================================================================
void Calculator::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Backspace:            // 退格
        handleBackspace();
        return;
    case Qt::Key_Escape:               // Esc / Delete 等同于清除
    case Qt::Key_Delete:
        handleClear();
        return;
    case Qt::Key_Return:               // 回车（主键盘和小键盘都算）
    case Qt::Key_Enter:
        handleEquals();
        return;
    default:
        break;
    }

    const QString text = event->text();
    if (text.isEmpty()) {              // 纯功能键（Shift、Ctrl 等）不处理
        QWidget::keyPressEvent(event);
        return;
    }

    const QChar ch = text.at(0);
    const bool isDigit = (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'));

    if (isDigit
        || ch == QLatin1Char('.') || ch == QLatin1Char(',')      // 小数点（含小键盘的逗号）
        || ch == QLatin1Char('+') || ch == QLatin1Char('-')
        || ch == QLatin1Char('*') || ch == QLatin1Char('/')
        || ch == QLatin1Char('x') || ch == QLatin1Char('X')
        || ch == QLatin1Char('=')) {
        handleToken(ch == QLatin1Char(',') ? QStringLiteral(".") : QString(ch));
        return;
    }

    if (ch == QLatin1Char('c') || ch == QLatin1Char('C')) {
        handleClear();
        return;
    }

    QWidget::keyPressEvent(event);     // 其余按键交给父类，避免影响系统快捷键
}

// =====================================================================
//  统一分发：鼠标和键盘都从这里进入
// =====================================================================
void Calculator::handleToken(const QString &token)
{
    if (token.isEmpty())
        return;

    const QChar first = token.at(0);
    const bool isDigit = (first >= QLatin1Char('0') && first <= QLatin1Char('9'));

    if (isDigit) {
        appendDigit(first);
    } else if (first == QLatin1Char('.')) {
        appendDot();
    } else if (first == QLatin1Char('=')) {
        handleEquals();
    } else if (token == QStringLiteral("C")) {
        handleClear();
    } else if (token == QStringLiteral("⌫")) {
        handleBackspace();
    } else {
        const QString op = normalizeOperator(first);
        if (!op.isEmpty())
            setOperator(op);
    }
}

// ---------------------------------------------------------------------
//  输入数字
// ---------------------------------------------------------------------
void Calculator::appendDigit(const QChar &digit)
{
    // 出错或刚出结果时按数字键：理解为“开始一次全新的计算”
    if (m_state == State::Error || m_state == State::ShowingResult)
        startNewInput();

    int digitCount = 0;
    for (int i = 0; i < m_input.size(); ++i) {
        if (m_input.at(i).isDigit())
            ++digitCount;
    }
    if (!m_freshInput && digitCount >= kMaxDigits)
        return;                              // 超过 15 位就忽略，防止显示溢出

    if (m_freshInput) {
        m_input = digit;                     // 新数字的第一位，直接覆盖
        m_freshInput = false;
    } else if (m_input == QStringLiteral("0")) {
        m_input = digit;                     // 0 后面直接输数字时替换，避免出现 007
    } else {
        m_input.append(digit);
    }

    updateDisplay();
}

// ---------------------------------------------------------------------
//  输入小数点：同一个操作数只允许有一个小数点
// ---------------------------------------------------------------------
void Calculator::appendDot()
{
    if (m_state == State::Error || m_state == State::ShowingResult)
        startNewInput();

    if (m_freshInput) {                      // 结果后 / 刚按运算符：从 "0." 开始
        m_input = QStringLiteral("0");
        m_freshInput = false;
    } else if (m_input.contains(QLatin1Char('.'))) {
        return;                              // 已经有点了，直接忽略 —— 避免 1..2
    }

    m_input.append(QLatin1Char('.'));        // ".5" 会被补成 "0.5"
    updateDisplay();
}

// ---------------------------------------------------------------------
//  按下运算符
// ---------------------------------------------------------------------
void Calculator::setOperator(const QString &op)
{
    if (m_state == State::Error)
        return;                              // 出错状态下必须先清除

    if (m_state == State::ShowingResult) {
        // 用上一次的结果继续算：结果本身已经是新的第一个操作数
        m_leftOperand = m_input.toDouble();
    } else if (m_state == State::EnteringLeft) {
        // 第一次按运算符：把当前输入记为第一个操作数
        m_leftOperand = m_input.toDouble();
    } else { // State::EnteringRight
        if (m_freshInput) {
            // 连续按运算符（如 5 + × 3）：只替换运算符，绝不重复计算
            m_operator = op;
            updateHistory();
            return;
        }
        // 第二个操作数已经输完，又按了运算符 —— 先把这一步算出来，实现连续运算
        if (!applyPendingOperation())
            return;                          // 除零等错误已在内部处理
    }

    m_operator = op;
    m_input = formatNumber(m_leftOperand);   // 显示区保留第一个操作数（和 Windows 计算器一致）
    m_freshInput = true;                     // 接下来输入的数字是新的一个数
    m_state = State::EnteringRight;

    updateHistory();
    updateDisplay();
}

// ---------------------------------------------------------------------
//  按下等号
// ---------------------------------------------------------------------
void Calculator::handleEquals()
{
    // 只有“第一个操作数 运算符 第二个操作数”齐全时才算
    // 例如 5 + = 、= =、刚出结果再按 =，都会被这里拦掉
    if (m_state != State::EnteringRight || m_freshInput)
        return;

    const QString expression = QStringLiteral("%1 %2 %3 =")
                                   .arg(formatNumber(m_leftOperand),
                                        displayOperator(m_operator),
                                        m_input);

    if (applyPendingOperation())
        ui->historyLabel->setText(expression);
}

// ---------------------------------------------------------------------
//  退格
// ---------------------------------------------------------------------
void Calculator::handleBackspace()
{
    if (m_state == State::Error || m_state == State::ShowingResult) {
        startNewInput();                     // 结果 / 错误状态下退格 == 清除
        return;
    }
    if (m_freshInput)
        return;                              // 还没开始输新数字（例如刚按完运算符），退格无效

    m_input.chop(1);                         // 删掉最后一个字符
    if (m_input.isEmpty() || m_input == QStringLiteral("-")) {
        m_input = QStringLiteral("0");
        m_freshInput = true;                 // 退到 0 以后，再输数字应当重新开始
    }
    updateDisplay();
}

// ---------------------------------------------------------------------
//  清除（C）
// ---------------------------------------------------------------------
void Calculator::handleClear()
{
    startNewInput();
}

// =====================================================================
//  内部辅助
// =====================================================================

// 执行挂起的运算：成功返回 true 并进入 ShowingResult 状态
bool Calculator::applyPendingOperation()
{
    if (m_operator.isEmpty())
        return false;

    const double right = m_input.toDouble();   // 第二个操作数
    double result = 0.0;

    if (m_operator == QStringLiteral("+")) {
        result = m_leftOperand + right;
    } else if (m_operator == QStringLiteral("-")) {
        result = m_leftOperand - right;
    } else if (m_operator == QStringLiteral("*")) {
        result = m_leftOperand * right;
    } else if (m_operator == QStringLiteral("/")) {
        if (qFuzzyIsNull(right)) {             // 除数为 0：不崩溃，给出提示
            showError(QStringLiteral("%1 ÷ 0  除数不能为 0").arg(formatNumber(m_leftOperand)));
            return false;
        }
        result = m_leftOperand / right;
    } else {
        return false;
    }

    // 结果放回“第一个操作数”，这样连续运算、结果后继续输入都能自然衔接
    m_leftOperand = result;
    m_input = formatNumber(result);
    m_freshInput = true;
    m_state = State::ShowingResult;
    updateDisplay();
    return true;
}

// 清空所有状态，回到“等待输入第一个操作数”
void Calculator::startNewInput()
{
    m_leftOperand = 0.0;
    m_operator.clear();
    m_input = QStringLiteral("0");
    m_freshInput = true;
    m_state = State::EnteringLeft;

    ui->historyLabel->clear();
    updateDisplay();
}

void Calculator::updateDisplay()
{
    ui->displayEdit->setText(m_input.isEmpty() ? QStringLiteral("0") : m_input);
}

void Calculator::updateHistory()
{
    if (m_operator.isEmpty()) {
        ui->historyLabel->clear();
    } else {
        ui->historyLabel->setText(QStringLiteral("%1 %2")
                                      .arg(formatNumber(m_leftOperand), displayOperator(m_operator)));
    }
}

void Calculator::showError(const QString &message)
{
    m_state = State::Error;
    m_operator.clear();
    m_input.clear();
    m_freshInput = true;

    ui->historyLabel->setText(message);
    ui->displayEdit->setText(QStringLiteral("错误"));
}

// 把 × ÷ - 之类的符号统一成内部使用的 * / -
QString Calculator::normalizeOperator(const QChar &ch)
{
    switch (ch.unicode()) {
    case '+':
        return QStringLiteral("+");
    case '-':
    case 0x2212:                 // − 数学减号
        return QStringLiteral("-");
    case '*':
    case 'x':
    case 'X':
    case 0x00D7:                 // × 乘号
        return QStringLiteral("*");
    case '/':
    case 0x00F7:                 // ÷ 除号
        return QStringLiteral("/");
    default:
        return QString();
    }
}

// 内部运算符反显成界面上的符号
QString Calculator::displayOperator(const QString &op)
{
    if (op == QStringLiteral("*"))
        return QStringLiteral("×");
    if (op == QStringLiteral("/"))
        return QStringLiteral("÷");
    return op;
}

// 浮点数显示：固定 10 位小数后去掉末尾的 0，避免 0.1+0.2 显示成 0.30000000000000004
QString Calculator::formatNumber(double value)
{
    if (qIsNaN(value) || qIsInf(value))
        return QStringLiteral("错误");

    QString text = QString::number(value, 'f', 10);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0')))
            text.chop(1);
        if (text.endsWith(QLatin1Char('.')))
            text.chop(1);
    }
    if (text == QStringLiteral("-0"))
        text = QStringLiteral("0");
    return text;
}
