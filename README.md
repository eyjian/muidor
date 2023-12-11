### 简单原理的极高性能分布式无单点高可用性唯一 ID 生成服务 Muidor

muidor 可单机部署，这种情况不需要 MuidorMaster，只需要启动 MuidorAgent 时指定“--label”参数的值即可。

涉及的 MySQL 库表，请参见 master.cpp 文件中的注释，在运行 MuidorMaster 之前，需要先创建一个库和三张表。

MuidorMaster 依赖 MySQL，在编译之前需要保证 MySQL 库已经正确安装到 /usr/local/mysql 目录下，且应当建立自目录 /usr/local/mysql/include/mysql，并将 /usr/local/mysql/include 下的头文件全复制到 /usr/local/mysql/include/mysql 目录下，这样才是一个标准规范的目录结构，在代码中是“#include <mysql/mysql.h>”这样引用的。

cmake 安装方法：
```
cmake -DCMAKE_INSTALL_PREFIX=<installation directory> .
```

“-DCMAKE_INSTALL_PREFIX=”后跟安装目录，如：

```
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr/local/muidor .
```

如果以 Debug 方式编译，指定参数：-DCMAKE_BUILD_TYPE=Debug

如果以 Release 方式编译，指定参数：-DCMAKE_BUILD_TYPE=Release

如果机器上没有 cmake 工具，则需要先安装好。对于可连接外网，并有 yum 的机器，只需要执行“yum install -y cmake”即可安装 cmake。

Muidor 是一个每秒可产生千万级唯一 ID 的服务，基于租约的思想，架构和实现均十分简单。MuidorAgent 和 MuidorMaster 均为单进程无线程结构，MuidorAgent 提供 ID 服务，MuidorMaster 维护 Label，

通过 Label 来唯一区别机器或节点，Label 为 1 字节数，最大支持 255 台机器同时提供服务，值为 0 的 Label 内部使用。

Muidor 为弱主从架构，具备高可用性，包含 MuidorMaster 在内的任意节点挂掉，均不影响服务。单个 MuidorAgent 提供超过 20 万/秒（CPU）取 8 字节无符号整数 ID 的能力，100 台机器可以提供 2000 多万/秒的服务，只占单个 CPU，无缓存低内存需求（单个 UDP 包大小为 28 字节，加上 IP 和 UDP 头为 56 字节）。支持一次批量取最多 100 个 ID，这样单台性能可大倍数提升。比如应用一次取 100 个，用完后再取，这样单台性能即可达到 2000 万个/秒。

应用场景：

1）8 字节整数的唯一 ID，可用于订单号等

2）各类包含日期和业务类型等的订单号、流水号、交易号等，如

如何保证 ID 的唯一性？

1）为每台机器分配唯一的 Label（标签），Uidor 的实现支持 Label 取值 1~255，也就是最多 255 台机器

2）每台机器自维护一个 4 字节无符号的循环递增 Sequence（序号）

3）唯一 ID 加上日期时间（年、月、日、小时等）

4）以上三部分组成即可保证唯一性。

使用租约管理 Label，以简化 Label 的维护，租期可定制，但建议以天为单位。
Muidor 各组成均为单进程无线程，成员间通讯采用 UDP 协议，以致实现轻巧但又高效。

日志控制：

可通过设置环境变量 MOOON_LOG_LEVEL 和 MOOON_LOG_SCREEN 来控制日志级别和是否在屏幕上输出日志。

1) MOOON_LOG_LEVEL 可以取值 debug,info,error,warn,fatal

2) MOOON_LOG_SCREEN 取值为 1 表示在屏幕输出日志，其它则表示不输出

工具说明：

1) MuidorCli 可用于测试 MuidorAgent，并可作为性能测试工具

2) MasterCli 可用于测试 Muidormaster，对于 Muidormaster 来说不需要考虑性能

参数设置建议：

1) Label 过期参数 expire 的值建议以天为单位进行设置，比如至少 1 天，建议为 1 周或更大，以便机器故障时，快乐休假

2) 请注意保持 master 和 agent 上的 Label 过期参数 expire 的值相同！！！

3) agent 的参数 interval 值，建议至少为 1 分钟，建议为 10 分钟或更大一点，要和 expire 的值保持合理关系，
     由于 UDP 不可靠，所以在一个 expire 周期内，最好保持 10 次以上的 interval，也就是 expire 的值最好是 interval 的 10 倍或更大

4) Muidormaster 的timeout 值要小于 UidorAgent 的 interval 值，以便及时的将值更新到 DB 中，比如可以为 interval 值的一半，或 6/10 等

5) MuidorAgent 的 steps 值最好不小于 10000，设置为 10 万会更佳，每重启一次 agent 进程，最多会浪费 steps 两倍的 sequence，因此太大也不好。
