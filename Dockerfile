FROM gcc:13
WORKDIR /app
COPY . .
RUN wget -q -O backend/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h \
 && g++ -O2 -std=c++17 -pthread backend/main.cpp -o energytrade
CMD ["./energytrade"]
