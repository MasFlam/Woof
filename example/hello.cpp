#include <woof/woof.hpp>
#include <iomanip>
#include <iostream>

struct MyMiddleware : public woof::MiddlewareI {
	
	size_t foo;
	
	virtual void
	before(woof::Request &req, woof::Response &resp) override
	{
		std::cout << std::quoted(req.target_string_raw()) << '\n';
		std::cout << "dupa 1\n";
		foo = req.headers().size();
	}
	
	virtual void
	after(woof::Request &req, woof::Response &resp) override
	{
		resp.headers().emplace("X-Foo", "Bar");
		std::cout << "dupa 4\n";
	}
};

struct MW2 : public woof::MiddlewareI {
	
	virtual void
	before(woof::Request &req, woof::Response &resp) override
	{
		std::cout << "dupa 2\n";
	}
	
	virtual void
	after(woof::Request &req, woof::Response &resp) override
	{
		std::cout << "dupa 3\n";
	}
};

// TODO: parsing multipart form data (bleh) et POST query params from body (ez)
// TODO: configurability -- req size limits, a Server: header
// TODO: auto insert version into code with cmake

int
main()
{
	woof::Server srv;
	
	srv.GET<"/hello/{lang}">(
		[](woof::Request &req, woof::Response &resp) {
			const std::string &lang = req.path()["lang"];
			
			if      (lang == "en") resp.body() << "Hello";
			else if (lang == "fr") resp.body() << "Salut";
			else if (lang == "pl") resp.body() << "Cześć";
			else if (lang == "ru") resp.body() << "Привет";
			else {
				resp.status(woof::status::NOT_FOUND);
				return;
			}
			
			auto a = req.query().get_or<int>("a", 42);
			auto b = req.query().get_or<int>("b");
			
			resp.body() << ", " << a + b << "!\n";
			
			std::cout << req.middleware<MyMiddleware>().foo << " headers\n";
		}
	);
	
	srv.GET<"/hello/**">(
		[](woof::Request &req, woof::Response &resp) {
			resp.body() << req.target_string_raw() << '\n';
		}
	);
	
	srv.add_middleware<MyMiddleware, "/hello/*">();
	srv.add_middleware<MW2, "/**">();
	
	srv.address("127.0.0.1").port(8888).run(2);
	return 0;
}
