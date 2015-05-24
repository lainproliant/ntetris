#!/usr/sbin/dtrace -s

#pragma D option quiet
#pragma D option defaultargs

#pragma D option switchrate=999hz

BEGIN
{

}

pid$target:a.out:handle_msg:entry
{
    self->ts = timestamp;
}

pid$target:a.out:handle_msg:return
{
    @counts[cpu] = count();
    @svc_time[cpu] = quantize(timestamp - self->ts);
}

profile:::tick-1m
{
    printa(@counts);
    printa(@svc_time);
    clear(@counts);
    clear(@svc_time);
}
