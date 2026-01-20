COMP = c++
FLAGS = -std=c++98
# FLAGS = -Wall -Wextra -Werror -std=c++98

NAME = ircserv

SRCS = main.cpp \
		ServerRead.cpp \
		IRCParser.cpp \
		Server.cpp \
		ServerBasicCommands.cpp \
		ServerChannel.cpp \
		ServerChannelOperator.cpp \
		ServerDispatcher.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(COMP) $(FLAGS) $(OBJS) -o $(NAME) && rm -f $(OBJS)

%.o: %.cpp
	$(COMP) $(FLAGS) -o $@ -c $<

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re