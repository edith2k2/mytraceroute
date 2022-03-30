#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
// #include <upd.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>

#include "ip.h"
#include "icmp.h"

#define DEST_PORT 32164
#define UDP_PORT 8080
#define PAYLOAD 52
#define BUFFER_SIZE 1000
#define TIMEOUT 1
#define MAX_HOPS 30

#define DEBUG 0
#define debug_print(fmt, ...) \
  do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
    __LINE__, __func__, ##__VA_ARGS__); } while (0)

void set_headers(struct iphdr*, struct udphdr*, int, struct hostent *);
void random_payload(char*);
void initialise_sockets(int*, int*);
unsigned short checksum(unsigned short*);
void print_bytes(char*);

extern int errno;

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    fprintf(stderr, "usage: getip address\n");
    exit(1);
  }
  struct hostent *h;
  h = gethostbyname(argv[1]);
  if (h == NULL)
  {
    herror("gethostbyname\n");
    exit(1);
  }
  // printf("Hostname : %s, ip : %s\n", argv[1], inet_ntoa(*((struct in_addr*)h -> h_addr)));

  int sockfd_udp, sockfd_icmp;
   // sock_icmp_addr;

  initialise_sockets(&sockfd_udp, &sockfd_icmp);
  // setting headers
  char buffer[BUFFER_SIZE], payload[PAYLOAD];
  int ttl = 1;
  int no_repeats = 0;
  fd_set master;
  int send_message = 1;
  clock_t start_time;
  // waiting on select call
  printf("mytraceroute to %s [%s] over a maximum of %d hops:\n", argv[1], inet_ntoa(*((struct in_addr*)h -> h_addr)), MAX_HOPS);
  while(1)
  {
    if (ttl > MAX_HOPS) break;
    struct iphdr *ip_hdr = (struct iphdr*)buffer;
    struct udphdr *udp_hdr = (struct udphdr*)(buffer + sizeof(struct iphdr));
    // printf("sent\n");
    random_payload(payload);
    set_headers(ip_hdr, udp_hdr, ttl, h);
    for (int i = 0; i < PAYLOAD; i++)
    {
      buffer[i + sizeof(struct iphdr) + sizeof(struct udphdr)] = payload[i];
    }
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr*)h -> h_addr)));
    dest_addr.sin_family = AF_INET;
    int send_ret = sendto(sockfd_udp, ip_hdr, ip_hdr -> tot_len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (send_ret < 0)
    {
      perror("Error in Sendto\n");
      exit(1);
    }
    start_time = clock();
    // ttl++;
    FD_ZERO(&master);
    FD_SET(sockfd_icmp, &master);
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    int select_ret = select(sockfd_icmp + 1, &master, 0, 0, &tv);
    if (select_ret == -1)
    {
      perror("Error in select call\n");
      exit(1);
    }else
    {
      debug_print("Enter else\n");
      if (FD_ISSET(sockfd_icmp, &master))
      {
        debug_print("Enter FD_ISSET\n");
        char buffer[BUFFER_SIZE];
        struct sockaddr_in sock_udp_addr;
        sock_udp_addr.sin_addr.s_addr = INADDR_ANY;
        sock_udp_addr.sin_port = htons(UDP_PORT);
        sock_udp_addr.sin_family = AF_INET;
        socklen_t sock_udp_addr_len = sizeof(sock_udp_addr);
        int recv_ret = recvfrom(sockfd_icmp, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&sock_udp_addr, &sock_udp_addr_len);
        clock_t recv_time = clock();
        if (recv_ret < 0)
        {
          debug_print("Error in recvfrom\n");
        }
        struct iphdr* ip_ret_hdr = (struct iphdr*)buffer;
        struct icmphdr* icmp_ret_hdr = (struct icmphdr*)(buffer + sizeof(struct iphdr));
        if (ip_ret_hdr -> protocol == 1)
        {
          debug_print("Icmp header with type %d\n", icmp_ret_hdr -> type);
          struct in_addr temp;
          temp.s_addr = ip_ret_hdr -> saddr;
          if (icmp_ret_hdr -> type == 3)
          {
            debug_print("DU\n");
            if (ip_ret_hdr -> saddr == inet_addr(inet_ntoa(*((struct in_addr*)h -> h_addr))))
            {
              printf("Hop_Count(%d)\t%s\t %.3f ms\n", ttl, inet_ntoa(temp), (float)(recv_time - start_time) / CLOCKS_PER_SEC * 1000);
              close(sockfd_udp);
              close(sockfd_icmp);
              exit(1);
            }
            exit(0);
          }else if (icmp_ret_hdr -> type == 11)
          {
            debug_print("TTE\n");
            printf("Hop_Count(%d)\t%s\t %.3f ms\n", ttl, inet_ntoa(temp), (float)(recv_time - start_time) / CLOCKS_PER_SEC * 1000);
            ttl++;
            no_repeats = 0;
            send_message = 1;
            continue;
          }
        }else
        {
        }
      }else
      {
        // timeout
        debug_print("Timedout\n");
        if (no_repeats == 3)
        {
          printf("Hop_Count(%d)\t*\t*\n", ttl);
          ttl++;
          no_repeats = 0;
        }else
        {
          no_repeats++;
        }
        send_message = 1;
      }
    }
  }
}

void initialise_sockets(int* sockfd_udp, int* sockfd_icmp)
{
  (*sockfd_udp) = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
  (*sockfd_icmp) = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (*sockfd_udp < 0)
  {
    perror("Error creation in sockfd_udp\n");
  }
  if (*sockfd_icmp < 0)
  {
    perror("Error creation in sockfd_icmp\n");
    exit(1);
  }
  struct sockaddr_in sock_udp_addr, sock_icmp_addr;
  sock_udp_addr.sin_addr.s_addr = INADDR_ANY;
  sock_udp_addr.sin_port = htons(8181);
  sock_udp_addr.sin_family = AF_INET;

  sock_icmp_addr.sin_addr.s_addr = INADDR_ANY;
  sock_icmp_addr.sin_port = htons(0);
  sock_icmp_addr.sin_family = AF_INET;
  int udpbind_res = bind(*sockfd_udp, (struct sockaddr*)&sock_udp_addr, sizeof(sock_udp_addr));
  if (udpbind_res < 0)
  {
    printf("%ld\n", sockfd_udp);
    perror("Error in bind of udp socket\n");
    exit(1);
  }
  // int icmpbind_res = bind(*sockfd_icmp, (struct sockaddr*)&sock_icmp_addr, sizeof(sock_icmp_addr));
  // if (icmpbind_res < 0)
  // {
  //   perror("Error in bind of icmp socket");
  //   exit(1);
  // }
  int optval = 1;
  int sockopt_res = setsockopt(*sockfd_udp, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval));
  if (sockopt_res < 0)
  {
    perror("Error in setsockopt\n");
    exit(1);
  }
}

void random_payload(char* payload)
{
  for (int i = 0; i < PAYLOAD; i++)
  {
    payload[i] = (rand() % 26) + 'A';
  }
  payload[PAYLOAD - 1] = '\0';
  debug_print("Payload is %s\n", payload);
}

void print_bytes(char* buffer)
{
  fflush(stdout);
  printf("\nIP header %d:", sizeof (struct iphdr));
  int i = 0, j = 0, k = 0;
  for (; i < sizeof(struct iphdr); i++)
  {
    printf("%c ", buffer[i]);
  }
  printf("\nUdp header %d:", sizeof(struct udphdr));
  for (; j < sizeof(struct udphdr); j++)
  {
    printf("%c ", buffer[i + j]);
  }
  printf("\n");
  printf("Payload is :");
  for (; k < 52; k++)
  {
    printf("%c", buffer[i + j + k]);
  }
  printf("\n");
}

unsigned short checksum(unsigned short* header)
{
  unsigned long sum;
  int size = sizeof(struct iphdr) + sizeof(struct udphdr);
  // print_bytes((char*)header);
  // debug_print("The size if %d", size);
  for (sum = 0; size > 0; size--)
      sum += *header++;
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return (unsigned short)(~sum);
}
void set_headers(struct iphdr* ip_hdr, struct udphdr* udp_hdr, int ttl, struct hostent *h)
{
  ip_hdr -> version = 4;
  ip_hdr -> ihl = 5;
  ip_hdr -> tos = 0;
  ip_hdr -> tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + PAYLOAD;
  ip_hdr -> id = htons(54322);
  ip_hdr -> ttl = ttl;
  ip_hdr -> protocol = 17; // udp
  ip_hdr -> saddr = 0;
  ip_hdr -> daddr = inet_addr(inet_ntoa(*((struct in_addr*)h -> h_addr)));

  udp_hdr -> uh_dport = htons(DEST_PORT);
  udp_hdr -> uh_sport = htons(UDP_PORT);
  udp_hdr -> uh_ulen = htons(sizeof(struct udphdr) + PAYLOAD);

  // print_bytes((char*)ip_hdr);
  ip_hdr -> check = checksum((unsigned short*)ip_hdr);
  // udp_hdr -> uh_sum = ;
}
