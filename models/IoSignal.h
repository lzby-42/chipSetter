/**
 * @file IoSignal.h
 * @brief IO信号数据模型 — 描述单个DI或DO通道
 */

#ifndef IOSIGNAL_H
#define IOSIGNAL_H

#include <QString>

/**
 * @enum IoType
 * @brief IO类型: 数字输入 或 数字输出
 */
enum IoType {
    IO_TYPE_DI,     // 数字输入 (Digital Input)
    IO_TYPE_DO      // 数字输出 (Digital Output)
};

/**
 * @struct IoSignal
 * @brief 单个IO通道的完整描述
 */
struct IoSignal {
    int     id;             // IO通道编号 (1-based, 对应GNC SDK)
    IoType  type;           // DI 或 DO
    QString name;           // 信号别名 ("晶盘X_Home", "红灯"等)
    int     value;          // 当前值 (0=低电平, 1=高电平)

    IoSignal() : id(0), type(IO_TYPE_DI), value(0) {}

    IoSignal(int id_, IoType type_, const QString& name_)
        : id(id_), type(type_), name(name_), value(0) {}
};

Q_DECLARE_METATYPE(IoSignal)

#endif // IOSIGNAL_H
