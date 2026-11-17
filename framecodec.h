#ifndef FRAMECODEC_H
#define FRAMECODEC_H

#include <QByteArray>
#include <QString>

#include "sample.h"

// 本工程自定义的串口帧格式（ASCII，一行一帧）：
//
//     $D,T=23.50,H=45.20*37\r\n
//     │   │            │  └── 结尾
//     │   │            └───── * + 两位十六进制校验（载荷逐字节 XOR）
//     │   └────────────────── 载荷：T=温度，H=湿度（湿度可选）
//     └────────────────────── 帧头
//
// 设备只要按这个格式发数据，上位机就能解析；
// 模拟器也调用 encode() 生成同样格式的数据，保证两条链路完全一致。
namespace FrameCodec
{
// 把温度/湿度编码成一帧。湿度传 qQNaN() 表示没有湿度数据。
QByteArray encode(double temperature, double humidity = qQNaN());

// 解析 "$D," 和 "*XX" 之间的载荷。成功返回 true。
bool decode(const QByteArray &payload, Sample &sample, QString *errorMessage = nullptr);

// 计算载荷的 XOR 校验值。
quint8 checksum(const QByteArray &payload);
}

#endif // FRAMECODEC_H
