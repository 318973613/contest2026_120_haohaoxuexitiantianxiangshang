/****************************************************************************
 * ui_status_bridge.c - System status source for the study terminal UI
 *
 * ai_agent does not publish a UI status file, so the values are read
 * straight from NuttX (sysinfo/uname/getifaddrs). This keeps the status
 * page working whether or not ai_agent is running.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "ui_status_bridge.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int ui_status_bridge_init(void)
{
  return 0;
}

/* Report the first non-loopback IPv4 address that is up. */

bool ui_status_read_network(char *buffer, size_t buffer_size)
{
  struct ifaddrs *interfaces = NULL;
  struct ifaddrs *item;
  bool found = false;

  if (buffer == NULL || buffer_size == 0)
    {
      return false;
    }

  if (getifaddrs(&interfaces) != 0)
    {
      return false;
    }

  for (item = interfaces; item != NULL; item = item->ifa_next)
    {
      struct sockaddr_in *address;
      char ip[INET_ADDRSTRLEN];

      if (item->ifa_addr == NULL ||
          item->ifa_addr->sa_family != AF_INET ||
          (item->ifa_flags & IFF_LOOPBACK) != 0 ||
          (item->ifa_flags & IFF_UP) == 0)
        {
          continue;
        }

      address = (struct sockaddr_in *)item->ifa_addr;
      if (address->sin_addr.s_addr == htonl(INADDR_ANY))
        {
          continue;
        }

      if (inet_ntop(AF_INET, &address->sin_addr, ip, sizeof(ip)) == NULL)
        {
          continue;
        }

      snprintf(buffer, buffer_size, "%s", ip);
      found = true;
      break;
    }

  freeifaddrs(interfaces);
  return found;
}

bool ui_status_read(ui_status_t *status)
{
  struct sysinfo system_info;
  struct utsname kernel_info;
  struct timespec monotonic;

  if (status == NULL)
    {
      return false;
    }

  memset(status, 0, sizeof(ui_status_t));
  status->version = 1;
  strncpy(status->network_ip, "未连接", sizeof(status->network_ip) - 1);
  strncpy(status->cpu_text, "CPU", sizeof(status->cpu_text) - 1);

  if (sysinfo(&system_info) == 0)
    {
      unsigned long long unit = system_info.mem_unit;
      unsigned long long total;
      unsigned long long freeram;

      if (unit == 0)
        {
          unit = 1;
        }

      total = (unsigned long long)system_info.totalram * unit;
      freeram = (unsigned long long)system_info.freeram * unit;

      status->mem_total_mb = (int)(total / (1024ULL * 1024ULL));
      status->mem_free_mb = (int)(freeram / (1024ULL * 1024ULL));
      status->mem_used_mb = status->mem_total_mb - status->mem_free_mb;
      status->uptime_sec = (int)system_info.uptime;
      status->cpu_count =
        system_info.procs > 0 ? (int)system_info.procs : 1;

      if (total > 0)
        {
          status->mem_used_percent =
            (float)((double)(total - freeram) * 100.0 / (double)total);
        }
    }
  else
    {
      /* sysinfo is optional in some configs; still show a live uptime. */

      if (clock_gettime(CLOCK_MONOTONIC, &monotonic) == 0)
        {
          status->uptime_sec = (int)monotonic.tv_sec;
        }

      status->cpu_count = 1;
    }

  if (uname(&kernel_info) == 0)
    {
      snprintf(status->cpu_text, sizeof(status->cpu_text), "%s / %d 核",
               kernel_info.machine, status->cpu_count);
    }
  else
    {
      snprintf(status->cpu_text, sizeof(status->cpu_text), "CPU / %d 核",
               status->cpu_count);
    }

  status->network_online =
    ui_status_read_network(status->network_ip, sizeof(status->network_ip));

  if (!status->network_online)
    {
      strncpy(status->network_ip, "未连接", sizeof(status->network_ip) - 1);
    }

  return true;
}
