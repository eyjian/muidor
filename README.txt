简单原理的极高性能分布式无单点高可用性唯一ID生成服务Muidor

涉及的MySQL库表，请参见master.cpp文件中的注释，在运行MuidorMaster之前，需要先创建一个库和三张表。

cmake安装方法：
cmake -DCMAKE_INSTALL_PREFIX=<installation directory> .

“-DCMAKE_INSTALL_PREFIX=”后跟安装目录，如/usr/local/mooon：
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr/local/mooon .

如果以debug方式编译，指定参数：-DCMAKE_BUILD_TYPE=Debug
如果以Release方式编译，指定参数：-DCMAKE_BUILD_TYPE=Release
如果机器上没有cmake工具，则需要先安装好。对于可连接外网，并有yum的机器，只需要执行：yum install -y cmake即可安装cmake。

Muidor是一个每秒可产生千万级唯一ID的服务，基于租约的思想，架构和实现均十分简单。
MuidorAgent和MuidorMaster均为单进程无线程结构，MuidorAgent提供ID服务，MuidorMaster维护Label，
通过Label来唯一区别机器，Label为1字节数，最大支持255台机器同时提供服务，值为0的Label内部使用。

Muidor弱主从架构，具备高可用性，包含MuidorMaster在内的任意节点挂掉，均不影响服务。
单个MuidorAgent提供超过20万/秒（CPU）取8字节无符号整数ID的能力，
100台机器可以提供2000多万/秒的服务，只占单个CPU，无缓存低内存需求（单个UDP包大小为28字节，加上IP和UDP头为56字节）。
支持一次批量取最多100个ID，这样单台性能可大倍数提升。比如应用一次取100个，用完后再取，这样单台性能即可达到2000万个/秒。

应用场景：
1）8字节整数的唯一ID，可用于订单号等
2）各类包含日期和业务类型等的订单号、流水号、交易号等，如

如何保证ID的唯一性？
1）为每台机器分配唯一的Label（标签），Uidor的实现支持Lable取值1~255，也就是最多255台机器
2）每台机器自维护一个4字节无符号的循环递增Sequence（序号）
3）唯一ID加上日期时间（年、月、日、小时等）
4）以上3部分组成即可保证唯一性。

使用租约管理Label，以简化Label的维护，租期可定制，但建议以天为单位。
Muidor各组成均为单进程无线程，成员间通讯采用UDP协议，以致实现轻巧但又高效。

日志控制：
可通过设置环境变量MOOON_LOG_LEVEL和MOOON_LOG_SCREEN来控制日志级别和是否在屏幕上输出日志
1) MOOON_LOG_LEVEL可以取值debug,info,error,warn,fatal
2) MOOON_LOG_SCREEN取值为1表示在屏幕输出日志，其它则表示不输出

工具说明：
1) MuidorCli可用于测试MuidorAgent，并可作为性能测试工具
2) MasterCli可用于测试Muidormaster，对于Muidormaster来说不需要考虑性能

参数设置建议：
1) Label过期参数expire的值建议以天为单位进行设置，比如至少1天，建议为1周或更大，以便机器故障时，快乐休假
2) 请注意保持master和agent上的Label过期参数expire的值相同！！！
3) agent的参数interval值，建议至少为1分钟，建议为10分钟或更大一点，要和expire的值保持合理关系，
     由于UDP不可靠，所以在一个expire周期内，最好保持10次以上的interval，也就是expire的值最好是interval的10倍或更大
4) Muidormaster的timeout值要小于UidorAgent的interval值，以便及时的将值更新到DB中，比如可以为interval值的一半，或6/10等
5) MuidorAgent的steps值最好不小于10000，设置为10万会更佳，每重启一次agent进程，最多会浪费steps两倍的sequence，因此太大也不好。
