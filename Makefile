NAME		=	webserv
CXX			=	c++
FLAGS		=	-Wall -Wextra -Werror -std=c++98
INCLUDES	=	-I includes
SRCS_DIR	=	srcs
OBJS_DIR	=	objs
SRCS		=	main.cpp RequestData.cpp ResponseData.cpp \
				ConfigData.cpp ServerBlock.cpp Transaction.cpp \
				EventManager.cpp Util.cpp \

OBJS		=	$(addprefix $(OBJS_DIR)/, $(SRCS:.cpp=.o))

all			:	$(NAME)

$(NAME)		:	$(OBJS)
	$(CXX) $(FLAGS) -o $(NAME) $(OBJS)

$(OBJS_DIR)/%.o	:	$(SRCS_DIR)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(FLAGS) $(INCLUDES) -c $< -o $@

clean		:
	rm -rf $(OBJS_DIR)

fclean		:	clean
	rm -f $(NAME)

re			:	fclean all

.PHONY		:	all clean fclean re
