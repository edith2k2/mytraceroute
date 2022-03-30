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

void set_headers(struct iphdr*, struct udphdr*, int, struct hostent *, int);
void random_payload(char*);
void initialise_sockets(int*, int*, struct sockaddr_in*);
int checksum(char*);

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
  printf("Hostname : %s, ip : %s\n", argv[1], inet_ntoa(*((struct in_addr*)h -> h_addr)));

  int sockfd_udp, sockfd_icmp;
  struct sockaddr_in dest_addr, sock_icmp_addr;
  dest_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr*)h -> h_addr)));
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(DEST_PORT);
  initialise_sockets(&sockfd_udp, &sockfd_icmp, &sock_icmp_addr);
  int ttl = 1;
  // setting headers
  char buffer[BUFFER_SIZE], payload[PAYLOAD];
  struct iphdr *ip_hdr = (struct iphdr*)buffer;
  struct udphdr *udp_hdr = (struct udphdr*)(buffer + sizeof(struct iphdr));

  int no_repeats = 0;
  fd_set master;
  int send_message = 1;
  // waiting on select call
  // while(1)
  // {
  //   if (ttl > 16) break;
  //   if (send_message)
  //   {
  //     random_payload(payload);
  //     set_headers(ip_hdr, udp_hdr, ttl, h, checksum(payload));
  //     strcpy(buffer + sizeof(ip_hdr) + sizeof(udp_hdr), payload);
  //     int send_ret = sendto(sockfd_udp, buffer, ip_hdr -> tot_len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
  //     if (send_ret < 0)
  //     {
  //       perror("Error in Sendto\n");
  //       exit(1);
  //     }
  //   }
  //   FD_ZERO(&master);
  //   FD_SET(sockfd_icmp, &master);
  //   struct timeval tv;
  //   tv.tv_sec = TIMEOUT;
  //   tv.tv_usec = 0;
  //   int select_ret = select(sockfd_icmp + 1, &master, 0, 0, &tv);
  //   if (select_ret == -1)
  //   {
  //     perror("Error in select call\n");
  //     exit(1);
  //   }else
  //   {
  //     if (FD_ISSET(sockfd_icmp, &master))
  //     {
  //       char buffer[BUFFER_SIZE];
  //       socklen_t sock_icmp_addr_len = sizeof(sock_icmp_addr);
  //       int recv_ret = recvfrom(sockfd_icmp, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&sock_icmp_addr, &sock_icmp_addr_len);
  //       if (recv_ret < 0)
  //       {
  //
  //       }
  //       struct iphdr* ip_ret_hdr = (struct iphdr*)buffer;
  //       struct icmphdr* icmp_ret_hdr = (struct icmphdr*)(buffer + sizeof(ip_ret_hdr));
  //       if (ip_ret_hdr -> protocol == 1)
  //       {
  //         if (icmp_ret_hdr -> type == 3)
  //         {
  //           if (ip_ret_hdr -> saddr == sock_icmp_addr.sin_addr.s_addr)
  //           {
  //             printf("Found\n");
  //           }
  //           exit(0);
  //         }else if (icmp_ret_hdr -> type == 11)
  //         {
  //           printf("Time exceeded\n");
  //           no_repeats = 0;
  //         }
  //       }else
  //       {
  //         printf("suspicious packet\n");
  //       }
  //     }else
  //     {
  //       // timeout
  //       if (no_repeats == 3)
  //       {
  //         printf("****");
  //         ttl++;
  //         no_repeats = 0;
  //       }else
  //       {
  //         no_repeats++;
  //       }
  //     }
  //   }
  // }
}

void initialise_sockets(int* sockfd_udp, int* sockfd_icmp,struct sockaddr_in* sock_icmp_addr)
{
  *sockfd_udp = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
  *sockfd_icmp = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (sockfd_udp < 0)
  {
    perror("Error creation in sockfd_udp\n");
  }
  if (sockfd_icmp < 0)
  {
    perror("Error creation in sockfd_icmp\n");
    exit(1);
  }
  struct sockaddr_in sock_udp_addr;
  sock_udp_addr.sin_addr.s_addr = INADDR_ANY;
  sock_udp_addr.sin_port = htons(UDP_PORT);
  sock_udp_addr.sin_family = AF_INET;

  sock_icmp_addr -> sin_addr.s_addr = INADDR_ANY;
  sock_icmp_addr -> sin_port = htons(0);
  sock_icmp_addr -> sin_family = AF_INET;
  int udpbind_res = bind(*sockfd_udp, (struct sockaddr*)&sock_udp_addr, sizeof(sock_udp_addr));
  if (udpbind_res < 0)
  {
    printf("%d\n", sockfd_udp);
    perror("Error in bind of udp socket\n");
    exit(1);
  }
  int icmpbind_res = bind(*sockfd_icmp, (struct sockaddr*)&sock_icmp_addr, sizeof(sock_icmp_addr));
  if (icmpbind_res < 0)
  {
    perror("Error in bind of icmp socket");
    exit(1);
  }
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
}

int checksum(char* header)
{
  unsigned long sum;
  int size = sizeof(struct iphdr) + sizeof(struct udphdr);
  for (sum = 0; size > 0; size--)
      sum += *header++;
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return (unsigned short)(~sum);
}
void set_headers(struct iphdr* ip_hdr, struct udphdr* udp_hdr, int ttl, struct hostent *h, int check_sum)
{
  ip_hdr -> version = 4;
  ip_hdr -> ihl = 5;
  ip_hdr -> tos = 0;
  ip_hdr -> tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + PAYLOAD;
  ip_hdr -> id = htons(444444);
  ip_hdr -> ttl = ttl;
  ip_hdr -> protocol = 17; // udp
  ip_hdr -> check = check_sum;
  ip_hdr -> saddr = 0;
  ip_hdr -> daddr = inet_addr(inet_ntoa(*((struct in_addr*)h -> h_addr)));

  udp_hdr -> uh_dport = htons(DEST_PORT);
  udp_hdr -> uh_sport = htons(UDP_PORT);
  udp_hdr -> uh_ulen = htons(sizeof(struct udphdr) + PAYLOAD);
  // udp_hdr -> uh_sum = ;
}
