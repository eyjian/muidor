#include "sys/report_self_decorator.h"
#include "sys/report_self.h"
#include "utils/args_parser.h"

// 指定ReportSelf的配置文件（值为空时表示不上报）
STRING_ARG_DEFINE(report_self_conf, "/etc/mooon_report_self.conf", "report self conf file, will not report if empty");

// ReportSelf上报间隔（单位：秒，值为0时表示不上报）
INTEGER_ARG_DEFINE(int, report_self_interval, 3600, 0, 36000000, "interval to report in seconds, will not report if 0");

SYS_NAMESPACE_BEGIN

void CReportSelfDecorator::start_report_self()
{
    // 启动上报，
    // 不关心返回值，因为report_conf指定的配置文件不一定存在
    if ((!argument::report_self_conf->value().empty()) &&
        (argument::report_self_interval->value() > 0))
    {
        (void)sys::start_report_self(argument::report_self_conf->value(), argument::report_self_interval->value());
    }
}

void CReportSelfDecorator::stop_report_self()
{
    sys::stop_report_self();
}

SYS_NAMESPACE_END
