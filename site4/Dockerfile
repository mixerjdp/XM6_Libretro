FROM nginx:alpine
COPY www/ /usr/share/nginx/html/
COPY health /usr/share/nginx/html/health
COPY nginx.conf /etc/nginx/conf.d/default.conf
EXPOSE 80
