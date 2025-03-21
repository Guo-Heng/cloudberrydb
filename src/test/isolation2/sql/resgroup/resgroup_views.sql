select *
  from gp_toolkit.gp_resgroup_config
 where groupname='default_group';

select groupname
     , groupid
     , num_running
     , num_queueing
     , num_queued
     , num_executed
  from gp_toolkit.gp_resgroup_status
 where groupname='default_group';

select groupname
     , groupid
     , cpu_usage
	 , memory_usage
  from gp_toolkit.gp_resgroup_status_per_host s
  join gp_segment_configuration c
    on s.hostname=c.hostname and c.content=-1 and role='p'
 where groupname='default_group';

select groupname
     , groupid
     , segment_id
     , vmem_usage
  from gp_toolkit.gp_resgroup_status_per_segment
 where groupname='default_group'
 order by segment_id;

select *
  from gp_toolkit.gp_resgroup_role
 where rrrolname='gpadmin';

-- also log the raw output of the views, if any of above tests failed it is
-- easier to find out the causes with these logs.

-- start_ignore
select * from gp_toolkit.gp_resgroup_config;
select * from gp_toolkit.gp_resgroup_status;
select * from gp_toolkit.gp_resgroup_status_per_host;
select * from gp_toolkit.gp_resgroup_status_per_segment;
-- end_ignore
