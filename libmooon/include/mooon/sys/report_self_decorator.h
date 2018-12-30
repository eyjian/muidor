#ifndef MOOON_APPLICATION_REPORT_SELF_DECORATOR_H
#define MOOON_APPLICATION_REPORT_SELF_DECORATOR_H
#include <mooon/sys/main_template.h>
SYS_NAMESPACE_BEGIN

class CReportSelfDecorator: public IReportSelf
{
private:
    virtual void start_report_self();
    virtual void stop_report_self();
};

SYS_NAMESPACE_END
#endif // MOOON_APPLICATION_REPORT_SELF_DECORATOR_H
