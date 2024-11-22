FROM gcc:latest as build
WORKDIR /usr/src/
RUN git clone https://github.com/clark15b/xupnpd2.git
WORKDIR /usr/src/xupnpd2
RUN make -f Makefile.linux all

FROM debian:12-slim
RUN apt update && apt install -y openssl 
COPY --from=build /usr/src/xupnpd2/xupnpd /opt/xupnpd2/
COPY --from=build /usr/src/xupnpd2/xupnpd.cfg /opt/xupnpd2/
COPY --from=build /usr/src/xupnpd2/xupnpd.lua /opt/xupnpd2/
COPY --from=build /usr/src/xupnpd2/www	/opt/xupnpd2/www
WORKDIR /opt/xupnpd2/

CMD ["/opt/xupnpd2/xupnpd"]
