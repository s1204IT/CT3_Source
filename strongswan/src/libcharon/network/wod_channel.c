#include "wod_channel.h"

#include <daemon.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <cutils/sockets.h>

#define	WOD_TCP_TIMEOUT		10

static int wod_tcp_txrx(char* txbuf, char *rxbuf, int rxbuf_size, int *rxlen)
{
	int sockfd = -1;
	int ret = -1;
	int txlen;
	struct sockaddr_in servaddr,cliaddr;
	struct timeval timeout = { .tv_sec = WOD_TCP_TIMEOUT, .tv_usec = 0 };

	if (txbuf == NULL || rxbuf == NULL || rxbuf_size <= 0 || rxlen == NULL)
	{
		DBG1(DBG_IKE, "Error: txbuf/rxbuf/rxlen is NULL or rxbuf_size <= 0");
		goto end;
	}

	*rxlen = 0;
   	sockfd = socket_local_client("wod_ipsec", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
	if (sockfd < 0) {
		DBG1(DBG_IKE, "Error: create android socket failed: %d", sockfd);
		goto end;
	}

	if ((txlen = strlen(txbuf)) > 0)
	{
		txlen++; /* include '\0' to be delimiter */
	}
	else
	{
		DBG1(DBG_IKE, "Error: invalid txlen=%d", txlen);
		goto end;
	}

	DBG1(DBG_IKE, "Tx to WOD - len:%d, data:[%s]", txlen, txbuf);
	if (send(sockfd, txbuf, txlen, 0) < 0) {
		DBG1(DBG_IKE, "Error: send failed");
		goto end;
	}

	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
		DBG1(DBG_IKE, "Error: setsockopt SO_RCVTIMEO failed");
	}

	ret = recv(sockfd, rxbuf, rxbuf_size - 1, 0);
	if (ret <= 0) {
		DBG1(DBG_IKE, "Error: %s", (ret == 0)? "session closed" : "recv failed or Timeout");
		ret = -1;
		goto end;
	}
	*rxlen = ret;

	DBG1(DBG_IKE, "RX from WOD - len:%d, data:[%s]", *rxlen, rxbuf);
	ret = 0;

end:
	if (sockfd >= 0) {
		close(sockfd);
	}
	return ret;
}

void notify_wod(wo_notify_cmd_t cmd, const char *value, conn_info_prop* prop)
{
	char rxbuf[MAX_BUF_LEN] = {0};
	char tmp[MAX_BUF_LEN] = {0};
	char addrs[MAX_BUF_LEN] = {0};
	char *p, *sim_sn_info = strdupnull(value);
	int rxlen = 0;
	int n_6, n_4;

	if (!sim_sn_info)
	{
		DBG1(DBG_IKE, "Error: Null sim_sn_info");
		return;
	}
	p = strchr(sim_sn_info, '_');
	if (p)
	{
		*p = ',';
	}
	else
	{
		DBG1(DBG_IKE, "Error: wrong format sim_sn_info: %s", sim_sn_info);
		free(sim_sn_info);
		return;
	}
	switch (cmd)
	{
		case N_ATTACH:
			snprintf(tmp, MAX_BUF_LEN, "ipsecattach=%s,%d,%d,%s,%s",
					sim_sn_info, prop->status, prop->sub_error, prop->ip6[0], prop->ip[0]);
			break;
		case N_DETACH:
			snprintf(tmp, MAX_BUF_LEN, "ipsecdetach=%s", sim_sn_info);
			break;
		case N_PCSCF:
			for (n_6 = 0; n_6 < MAX_PCSCF_NUM && prop->pcscf6[n_6][0]; ++n_6)
			{
				snprintf(addrs, MAX_BUF_LEN, "%s,%s", addrs, prop->pcscf6[n_6]);
			}
			for (n_4 = 0; n_4 < MAX_PCSCF_NUM && prop->pcscf[n_4][0]; ++n_4)
			{
				snprintf(addrs, MAX_BUF_LEN, "%s,%s", addrs, prop->pcscf[n_4]);
			}
			snprintf(tmp, MAX_BUF_LEN, "ipsecpcscf=%s,%d,%d%s", sim_sn_info, n_6, n_4, addrs);
			break;
		case N_DNS:
			for (n_6 = 0; n_6 < MAX_DNS_NUM && prop->dns6[n_6][0]; ++n_6)
			{
				snprintf(addrs, MAX_BUF_LEN, "%s,%s", addrs, prop->dns6[n_6]);
			}
			for (n_4 = 0; n_4 < MAX_DNS_NUM && prop->dns[n_4][0]; ++n_4)
			{
				snprintf(addrs, MAX_BUF_LEN, "%s,%s", addrs, prop->dns[n_4]);
			}
			snprintf(tmp, MAX_BUF_LEN, "ipsecdns=%s,%d,%d%s", sim_sn_info, n_6, n_4, addrs);
			break;
		case N_OOS_START:
			snprintf(tmp, MAX_BUF_LEN, "ipsecoos=%s,1", sim_sn_info);
			break;
		case N_OOS_END:
			snprintf(tmp, MAX_BUF_LEN, "ipsecoos=%s,0", sim_sn_info);
			break;
		default:
			break;
	}

	wod_tcp_txrx(tmp, rxbuf, MAX_BUF_LEN, &rxlen);
	free(sim_sn_info);
}

void notify_wod_attach_failed(const char *value, int status, int sub_error)
{
	conn_info_prop prop;

	memset(&prop, 0, sizeof(conn_info_prop));
	prop.status = status ? status : PDN_STATUS_UNABLE_MAKE_IPSEC_TUNNEL;
	prop.sub_error = sub_error;
	notify_wod(N_ATTACH, value, &prop);
}

int atcmd_txrx(char *txbuf, char *rxbuf, int rxbuf_size, int *rxlen)
{
	int ret;

	ret = wod_tcp_txrx(txbuf, rxbuf, rxbuf_size, rxlen);
	if (ret < 0)
	{
		*rxlen = 0;
		goto end;
	}
	/* Decrease rxlen if the end of string is '\0' */
	if (*rxlen > 0 && rxbuf[*rxlen - 1] == '\0')
	{
		(*rxlen)--;
	}

end:
	rxbuf[*rxlen] = 0;
	return ret;
}

