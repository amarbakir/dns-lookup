/* Authors: Amar Bakir, Shashank Seeram */
/* CS 352 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFSIZE 2048
#define PORT 1112

/* 
	DNS structures
*/

/* dns header - see rfc1035 */
/* this is the main header of a DNS message */
/* it is followed by zero or more questions, answers, authorities, and additional sections */
/* the last four count fields tell you how many of each to expect */

typedef struct {
	unsigned short id;
	unsigned char rd:1;
	unsigned char tc:1;
	unsigned char aa:1;
	unsigned char opcode:4;
	unsigned char qr:1;
	unsigned char rcode:4;
	unsigned char cd:1;
	unsigned char ad:1;
	unsigned char z:1;
	unsigned char ra:1;
	unsigned short qd_count;
	unsigned short an_count;
	unsigned short ns_count;
	unsigned short ar_count;
} dns_header;

/* dns question section format. This is prepended with a name */
/* check the specs for the format of a name. Instead of components */
/* separated by dots, each component is prefixed with a byte containing */
/* the length of that component */

typedef struct {
	unsigned short qtype;
	unsigned short qclass;
} dns_question;

/* DNS resource record format */
/* The answer, authority, and additional sections all share this format. */
/* It is prepended with a name and suffixed with additional data */

typedef struct __attribute__ ((__packed__)) {
	unsigned short type;
	unsigned short class;
	unsigned int ttl;
	unsigned short data_len;
} dns_rrhdr;

/*
	Communication and data storage fields.
*/

/* Node struct to record entry. */
typedef struct Node {
	char *domain;
	unsigned int ipaddr;
	struct node *next;
} node;

/* Entry point for entries starting with a certain character (a-z). */
typedef struct RecordKeeper {
	int numEntries;
	node *first;
} recordkeeper;

/*typedef struct {
	struct recordkeeper db[26];
} dnsdb;*/

recordkeeper myDB[26];				/* our host-address daabase */
struct sockaddr_in myaddr;			/* our address */
struct sockaddr_in remoteaddr;			/* remote address */
socklen_t addrlen = sizeof(remoteaddr);		/* length of addresses */
int recvlen;					/* # bytes received */
int fd;                         		/* our socket */
unsigned char buff[BUFFSIZE];     		/* receive buffer */
int port = PORT;				/* port number */

/*
	Inserts a new node into the DB
 */
int insertRecord(node *newNode) {
	//printf("Inserting node for %s with address 0x%08x.\n", newNode->domain, newNode->ipaddr);
	/*if (newNode == NULL) {
		printf("Error: cannot insert NULL entry.\n");
		return 0;
	}*/
	int index = (int) (*(newNode->domain) - 'a');
	if ((myDB[index].numEntries == 0) || (myDB[index].first == NULL)) {
		myDB[index].first = newNode;
		myDB[index].first->next = newNode;
		newNode->next = NULL;
	} else {
		newNode->next = myDB[index].first;
		myDB[index].first = newNode;
	}
	myDB[index].numEntries++;
	return 1;
}

/*
	Search for a record in the DB
 */
node *searchRecord(char *domain) {
	//printf("Searching for node.\n");
	int index = (int) ((*domain) - 'a');
	if ((myDB[index].numEntries == 0) || (myDB[index].first == NULL)) {
		//printf("No entry for this starting char.\n");
		return NULL;
	} else {
		//printf("%d entries exist for this starting char.\n", myDB[index].numEntries);
		node *curr;
		curr = myDB[index].first;
		while (curr != NULL) {
			if (strcmp(curr->domain, domain) == 0) {
				//printf("Returning answer.\n");
				return curr;
			}
			curr = curr->next;
		}
		//printf("No answer found.\n");
		return NULL;
	}
}

/*
	Initializes DB
 */
void initDB() {
	//printf("Initializing DB.\n");
	int i = 0;
	for (i = 0; i < 26; i++) {
		myDB[i].numEntries = 0;
		myDB[i].first = NULL;
	}
}

/*
	Reads host entries from file into DB.
 */
int readHostList(char *fname) {
	//printf("Reading Host List.\n");
	FILE *fp;
	fp = fopen(fname, "r");
	if (fp == NULL) {
		printf("Error: could not read hosts file.\n");
		return 0;
	} else {
		char line[100];
		
		while (fgets(line, 100, fp) != NULL) {
			//printf("Reading line: %s\n", line);
			if (isdigit(line[0])) {
				char tempIP[4];
				unsigned int IP;
				int count = 0;
				int section = 3;
				node *tempnode;
				char *tempdomain;
				int place;
				
				tempnode = malloc(sizeof(node));
				memset(tempnode, 0, sizeof(tempnode));
				tempnode->ipaddr = 0;
				tempnode->next = NULL;
				
				//printf("Starting line parse.\n");
				//printf("Finding address.\n");
				int i = 0;
				for (i = 0; i < strlen(line); i++) {
					if (isdigit(line[i])) {
						tempIP[count] = line[i];
						count++;
					} else if (line[i] == '.') {
						tempIP[count] = '\0';
						tempnode->ipaddr += ((unsigned int) strtol(tempIP, (char **)NULL, 10)) << (section * 8);
						count = 0;
						section--;
					} else if (isspace(line[i])) {
						if (count != 0) {
							tempIP[count] = '\0';
							tempnode->ipaddr += ((unsigned int) strtol(tempIP, (char **)NULL, 10)) << (section * 8);
							count = 0;
							place = i + 1;
							break;
						}
					}
				}
				//printf("Address is %d\n", tempnode->ipaddr);

				//printf("Finding domain.\n");
				int start = 0;
				int end = 0;
				int j = 0;
				for (j = place; j <= strlen(line); j++) {
					if (isalnum(line[j]) && (start == 0)) {
						start = j;
					} else if ((!(isalnum(line[j]) || (line[j] == '.')) && (start != 0)) || (line[j] == '\0') || (line[j] == '\n')) {
						end = j - 1;
						int dlength = (end - start + 2);
						tempnode->domain = malloc(sizeof(char) * (end - start + 2));
						strncpy(tempnode->domain, (line + start), (end - start + 1));
						tempnode->domain[(end - start + 1)] = '\0';
						break;
					}
				}
				//printf("Domain is %s\n", tempnode->domain);

				if(insertRecord(tempnode) == 0) {
					printf("Error: insertRecord failed.\n");
					return(0);
				}
			}
		}
	}
	printf("Hosts Entry Complete.\n");
	return(1);
}

int main(int argc, char **argv) {
	static char usage[] = "Usage: dns-server [-p port#] [-f hostfile]\n";
	
	if ((argc != 1) && (argc != 3) && (argc != 5)) {
		printf("Error: invalid usage.\n");
		printf(usage);
		return(1);
	}
	
	extern char *optarg;
	extern int optind;
	int c, err = 0; 
	int pflag=0, fflag=0;
	char *fname = "hosts.txt";

	while ((c = getopt(argc, argv, "f:p:")) != -1) {
		switch (c) {
		case 'p':
			pflag = 1;
			port = (int) strtol(optarg, (char **)NULL, 10);
			if ((port > 65535) || (port < 1024)) {
				printf("Error: invalid port number.\n");
				return(1);
			}
			break;
		case 'f':
			fflag = 1;
			fname = optarg;
			break;
		case '?':
			err = 1;
			break;
		}
	}
	
	initDB();
	if (readHostList(fname) == 0) {
		return 1;
	}
	
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Error: cannot create socket.\n");
		//perror("socket creation failed");
		return 1;
	}

	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(port);
	
	memset((char *)&remoteaddr, 0, addrlen);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		printf("Error: bind failed.\n");
		//perror("bind failed");
		return 1;
	}
	
	for (;;) {
		printf("Waiting on connection.\n");
        	recvlen = recvfrom(fd, buff, BUFFSIZE, 0, (struct sockaddr *)&remoteaddr, &addrlen);
        	if (recvlen > 0) {
			buff[recvlen] = '\0';
			dns_header *dns = (dns_header *)buff;
			
			if ((dns->qr == 0) && (dns->opcode == 0) && (ntohs(dns->qd_count) >= 1))  {
				char *curr;
				curr = (buff + sizeof(dns_header));
				char rawquery[128] = "";
				char query[64] = "";
				char *respos;
				
				strncat(rawquery, curr, 1);
				int rawtotal = 1;
				int length = (int) *curr;
				int total = length;
				curr += 1;
				
				while ((int) (*curr) != 0) {
					strncat(rawquery, curr, 1);
					rawtotal++;
					
					if (isalnum(*curr) && length > 0) {
						strncat(query, curr, 1);
						length--;
					} else {
						length = (int) *curr;
						if (length == 0) {
							break;
						}
						total += length;
						total++;
						strncat(query, ".", 1);
					}
					
					curr += 1;
				}
				
				respos = curr + 1;
				strncat(rawquery, curr, 1);
				rawtotal++;
				query[total] = '\0';
				
				curr += 1;
				dns_question *quest = (dns_question *)curr;
				if (ntohs(quest->qtype) == 1 && ntohs(quest->qclass) == 1) {
					node *result;
					
					if ((result = searchRecord(query)) != NULL) {
						printf("Found record for %s\n", query);
						dns_rrhdr *answer = (dns_rrhdr *) malloc(sizeof(dns_rrhdr));
						//respos = curr + sizeof(dns_question);
						
						answer->type = htons(1);
						answer->class = htons(1);
						answer->ttl = htonl(64);
						answer->data_len = htons(4);
						dns->qr = 1;
						dns->aa = 1;
						dns->tc = 0;
						dns->ra = 0;
						dns->rcode = 0;
						dns->qd_count = htons(0);
						dns->an_count = htons(1);
						
						curr = respos;
						memcpy(curr, answer, sizeof(dns_rrhdr));
						curr += sizeof(dns_rrhdr);

						unsigned int answeraddr = htonl(result->ipaddr);

						//printf("Answer is 0x%08x.\n", result->ipaddr);
						memcpy(curr, &answeraddr, sizeof(answeraddr));
						curr += sizeof(answeraddr);
						
						sendto(fd, buff, (sizeof(dns_header) + rawtotal + sizeof(dns_rrhdr) + sizeof(answeraddr)), 0, (struct sockaddr *)&remoteaddr, addrlen);
					} else {
						printf("Record not found for %s\n", query);
						dns->qr = 1;
						dns->aa = 1;
						dns->tc = 0;
						dns->ra = 0;
						dns->rcode = 3;
						
						sendto(fd, buff, recvlen, 0, (struct sockaddr *)&remoteaddr, addrlen);
					}
				}
			} else if (dns->opcode != 0) {
				dns->qr = 1;
				dns->aa = 1;
				dns->tc = 0;
				dns->ra = 0;
				dns->rcode = 4;
				
				sendto(fd, buff, recvlen, 0, (struct sockaddr *)&remoteaddr, addrlen);
			}
			memset((char *)&buff, 0, BUFFSIZE);
        	}
    	}
	close(fd);
	return(0);
}
