NAME = ircserv

SRCS = main.cpp \
		Server.cpp

OBJS = $(SRCS:.cpp=.o)

COMP = c++
FLAGS = -Wall -Wextra -Werror -std=c++98

all: $(NAME)

$(NAME): $(OBJS)
	$(COMP) $(FLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(COMP) $(FLAGS) -o $@ -c $<

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re